#include "Simulation.h"
#include "physics/Ballistic.h"
#include "physics/Interceptor.h"
#include <QtMath>
#include <QSet>

static constexpr float PI = 3.14159265f;

Simulation::Simulation(QObject* parent) : QObject(parent)
{
    // Multi-layer defense: THAAD high-alt, PAC-3 medium, Iron Dome low-alt
    m_batteries = {
        {{ 0,     10000, 0}, "THAAD-1",  WeaponSystem::THAAD,
          8,  8, 3000.f, 150.f, 40000.f, 150000.f, 0.f},
        {{ 12000, -6000, 0}, "PAC3-A",   WeaponSystem::PAC_3,
          8,  8, 2500.f,  75.f,  5000.f,  40000.f, 0.f},
        {{-12000, -6000, 0}, "PAC3-B",   WeaponSystem::PAC_3,
          8,  8, 2500.f,  75.f,  5000.f,  40000.f, 0.f},
        {{ 4000,   4000, 0}, "IDOME-1",  WeaponSystem::IRON_DOME,
          20, 20,  350.f,  10.f,    50.f,  10000.f, 0.f},
    };

    m_game = new GameState(this);
    connect(m_game, &GameState::launchRequested,
            this,   &Simulation::onGameLaunchRequested);

    connect(&m_timer, &QTimer::timeout, this, &Simulation::tick);
    m_timer.start(16);
}

bool Simulation::hasRadarContact() const
{
    for (const auto& m : m_missiles)
        if (m.type == MissileType::Target && m.state == MissileState::Flying
            && m.pos.length() <= RADAR_RANGE)
            return true;
    return false;
}

bool Simulation::hasActiveInterceptors() const
{
    for (const auto& m : m_missiles)
        if (m.type == MissileType::Interceptor && m.state == MissileState::Flying)
            return true;
    return false;
}

// ── Game-mode threat launch ────────────────────────────────────────
void Simulation::onGameLaunchRequested(ThreatType type, float azimuth)
{
    launchThreatType(type, azimuth);
}

Missile Simulation::makeMissileFromThreat(ThreatType type, float azimuth)
{
    const ThreatSpec& sp = getThreatSpec(type);
    Missile m;
    m.id          = m_nextId++;
    m.type        = MissileType::Target;
    m.threat      = type;
    m.mass        = sp.mass;
    m.diameter    = sp.diameter;
    m.cd0         = sp.cd0;
    m.maneuvering = sp.maneuvering;

    float azRad = qDegreesToRadians(azimuth);
    float elRad = qDegreesToRadians(sp.elevation);
    float launchDist = sp.launchDist * 1000.f;

    m.pos = QVector3D(-qSin(azRad) * launchDist,
                      -qCos(azRad) * launchDist,
                      0.f);
    float hSpeed = sp.speed * qCos(elRad);
    float vSpeed = sp.speed * qSin(elRad);
    m.vel = QVector3D(hSpeed * qSin(azRad), hSpeed * qCos(azRad), vSpeed);
    return m;
}

void Simulation::launchThreatType(ThreatType type, float azimuth)
{
    Missile m = makeMissileFromThreat(type, azimuth);
    m_missiles.push_back(m);
    const ThreatSpec& sp = getThreatSpec(type);
    emit eventLogged(QString("THREAT #%1 [%2] — az %3°  v=%4 m/s")
                     .arg(m.id).arg(sp.name)
                     .arg(azimuth, 0, 'f', 1)
                     .arg(sp.speed, 0, 'f', 0));
}

// ── Manual launch (free-play) ─────────────────────────────────────
void Simulation::launchTarget(QVector3D impactHint)
{
    Missile m;
    m.id       = m_nextId++;
    m.type     = MissileType::Target;
    m.threat   = ThreatType::SCUD_B;
    m.mass     = m_params.mass;
    m.diameter = m_params.diameter;
    m.cd0      = m_params.cd0;

    float azDeg = m_params.azimuth;
    if (!impactHint.isNull())
        azDeg = qRadiansToDegrees(qAtan2(impactHint.x(), impactHint.y()));

    float azRad = qDegreesToRadians(azDeg);
    float elRad = qDegreesToRadians(m_params.elevation);

    const float launchDist = 250000.f;
    m.pos = QVector3D(-qSin(azRad) * launchDist,
                      -qCos(azRad) * launchDist,
                      0.f);

    float hSpeed = m_params.speed * qCos(elRad);
    float vSpeed = m_params.speed * qSin(elRad);
    m.vel = QVector3D(hSpeed * qSin(azRad), hSpeed * qCos(azRad), vSpeed);

    m.maneuvering = m_params.maneuvering;
    m_missiles.push_back(m);
    emit eventLogged(QString("TARGET #%1 — az %2° el %3° v=%4 m/s%5")
                     .arg(m.id)
                     .arg(azDeg, 0, 'f', 1)
                     .arg(m_params.elevation, 0, 'f', 1)
                     .arg(m_params.speed, 0, 'f', 0)
                     .arg(m_params.maneuvering ? "  [MANEUVERING]" : ""));
}

void Simulation::launchInterceptorAt(int targetId, int batteryIdx)
{
    if (batteryIdx < 0 || batteryIdx >= m_batteries.size()) return;
    LaunchBattery& bty = m_batteries[batteryIdx];

    if (bty.ammo <= 0) {
        emit eventLogged(QString("%1: WINCHESTER — no interceptors remaining").arg(bty.name));
        return;
    }
    if (bty.reloadTimer > 0.f) return;

    const Missile* tgt = nullptr;
    for (const auto& m : m_missiles)
        if (m.id == targetId && m.state == MissileState::Flying) { tgt = &m; break; }
    if (!tgt) return;

    // Altitude band check
    float alt = tgt->pos.z();
    if (alt < bty.minAlt || alt > bty.maxAlt) return;

    float T_go = 0.f;
    QVector3D ip = Interceptor::predictIntercept(bty.pos, bty.intSpeed * 0.80f,
                                                  tgt->pos, tgt->vel, &T_go);
    if (T_go < REACTION_TIME) return;

    Missile intm;
    intm.id            = m_nextId++;
    intm.type          = MissileType::Interceptor;
    intm.mass          = 90.f;
    intm.diameter      = 0.255f;
    intm.cd0           = 0.1f;
    intm.killRadius    = bty.killRad;
    intm.pos           = bty.pos;
    intm.targetId      = targetId;
    intm.launchBattery = batteryIdx;
    intm.vel           = (ip - bty.pos).normalized() * bty.intSpeed;
    m_missiles.push_back(intm);
    bty.ammo--;

    const char* wsName = (bty.wsys == WeaponSystem::THAAD)     ? "THAAD"
                       : (bty.wsys == WeaponSystem::IRON_DOME) ? "IRON-DOME"
                                                                : "PAC-3";
    emit eventLogged(
        QString("%1 %2 [ammo:%3]: #%4→TGT#%5  T_go=%6s  alt=%7km")
        .arg(wsName).arg(bty.name).arg(bty.ammo)
        .arg(intm.id).arg(targetId)
        .arg(T_go, 0, 'f', 1)
        .arg(ip.z() / 1000.f, 0, 'f', 1));
    emit ammoBatteryUpdated(batteryIdx, bty.ammo, bty.maxAmmo);
}

void Simulation::updateReload(float dt)
{
    for (int i = 0; i < m_batteries.size(); ++i) {
        LaunchBattery& b = m_batteries[i];
        if (b.reloadTimer <= 0.f) continue;
        b.reloadTimer -= dt;
        if (b.reloadTimer <= 0.f) {
            b.reloadTimer = 0.f;
            b.ammo = b.maxAmmo;
            emit ammoBatteryUpdated(i, b.ammo, b.maxAmmo);
            emit eventLogged(QString("%1: RELOADED — %2 ready").arg(b.name).arg(b.ammo));
        }
    }
}

void Simulation::checkMirvSplit()
{
    for (auto& m : m_missiles) {
        if (m.type != MissileType::Target
            || m.state != MissileState::Flying
            || m.threat != ThreatType::MIRV
            || m.milvSplit) continue;

        bool ascending = (m.vel.z() > 0.f);
        if (m.wasAscending && !ascending) {
            m.milvSplit = true;
            m.state     = MissileState::Exploded;

            const QVector3D spreadDir = QVector3D(m.vel.x(), m.vel.y(), 0).normalized();
            const QVector3D perpDir   = QVector3D(-spreadDir.y(), spreadDir.x(), 0);

            for (int i = -1; i <= 1; ++i) {
                Missile sub = makeMissileFromThreat(ThreatType::SCUD_B, 0.f);
                sub.id    = m_nextId++;
                sub.pos   = m.pos;
                sub.vel   = m.vel * 0.95f + perpDir * float(i) * 150.f;
                sub.trail.clear();
                m_spawnQueue.push_back(sub);
            }

            emit eventLogged(
                QString("MIRV #%1 SPLIT at alt=%2 km — 3 sub-warheads inbound")
                .arg(m.id).arg(m.pos.z() / 1000.f, 0, 'f', 1));
            emit mirvSplit(m.id, m.pos, m.vel);
        }
        m.wasAscending = ascending;
    }
}

void Simulation::reset()
{
    m_missiles.clear();
    m_explosions.clear();
    m_spawnQueue.clear();
    m_radarAcquired.clear();
    m_totalFired.clear();
    for (auto& b : m_batteries) { b.ammo = b.maxAmmo; b.reloadTimer = 0.f; }
    if (m_game) m_game->reset();
    m_time   = 0.f;
    m_nextId = 0;
    emit updated();
}

void Simulation::togglePause() { m_paused = !m_paused; }

void Simulation::tick()
{
    if (m_paused) return;
    for (int i = 0; i < m_speedMult; ++i)
        stepPhysics(DT);
    emit updated();
}

void Simulation::stepPhysics(float dt)
{
    m_time += dt;

    for (auto& m : m_missiles)
        if (m.state == MissileState::Flying)
            stepMissile(m, dt);

    checkMirvSplit();

    // Flush sub-warhead spawn queue (adding during loop would invalidate iterators)
    if (!m_spawnQueue.isEmpty()) {
        for (auto& sm : m_spawnQueue)
            m_missiles.push_back(sm);
        m_spawnQueue.clear();
    }

    if (m_autoIntercept)
        detectAndAutoLaunch();
    checkInterceptions();

    updateReload(dt);

    // Tick game state
    if (m_game && m_game->active()) {
        int flyingTargets = 0;
        for (const auto& m2 : m_missiles)
            if (m2.type == MissileType::Target && m2.state == MissileState::Flying)
                ++flyingTargets;
        m_game->tick(dt, flyingTargets);
    }

    for (auto& ex : m_explosions) {
        ex.age += dt;
        if (ex.age >= ex.duration) ex.active = false;
    }
    m_explosions.erase(
        std::remove_if(m_explosions.begin(), m_explosions.end(),
                       [](const Explosion& e){ return !e.active; }),
        m_explosions.end());
}

void Simulation::stepMissile(Missile& m, float dt)
{
    BallisticParams bp;
    bp.mass      = m.mass;
    bp.diameter  = m.diameter;
    bp.cd0       = m.cd0;
    bp.latitude  = m_params.latitude;
    bp.windSpeed = m_params.windSpeed;
    bp.windDir   = m_params.windDir;

    if (m.type == MissileType::Target) {
        Ballistic::stepRK4(m.pos, m.vel, dt, bp);
    } else {
        const Missile* tgt = nullptr;
        for (const auto& other : m_missiles)
            if (other.id == m.targetId && other.state == MissileState::Flying)
            { tgt = &other; break; }

        if (tgt)
            Interceptor::stepRK4(m.pos, m.vel, dt, tgt->pos, tgt->vel, m.age,
                                 5.f, 300.f, 600.f);
        else
            Ballistic::stepRK4(m.pos, m.vel, dt, bp); // free-fly
    }

    m.age += dt;

    // Terminal-phase maneuver for targets
    if (m.type == MissileType::Target && m.maneuvering)
        updateManeuver(m, dt);

    // Trail — add point every ~0.15s simulation time
    if (m.trail.isEmpty() || (m.pos - m.trail.last()).length() > 500.f)
        m.trail.push_back(m.pos);
    if (m.trail.size() > 500)
        m.trail.removeFirst();

    // Ground impact
    if (m.pos.z() < 0.f && m.age > 0.5f) {
        m.pos.setZ(0.f);
        if (m.type == MissileType::Target) {
            m.state = MissileState::Missed;
            addExplosion(m.pos);
            if (m_game) m_game->onGroundImpact(m.pos);
            emit eventLogged(QString("IMPACT: %1 #%2 — ground strike")
                             .arg(getThreatSpec(m.threat).name).arg(m.id));
        } else {
            m.state = MissileState::Missed;
        }
    }
}

void Simulation::detectAndAutoLaunch()
{
    // IMPORTANT: collect launch orders first, then fire — launchInterceptorAt
    // calls m_missiles.push_back() which invalidates iterators.
    struct Order { int targetId; int batteryIdx; };
    QVector<Order> orders;

    // Networked radar: target is "seen" if within RADAR_RANGE of ANY battery.
    // (Real Patriot uses a shared AN/MPQ-65 radar — all batteries share the picture.)
    const int n = m_missiles.size();

    for (int ti = 0; ti < n; ++ti) {
        const Missile& tgt = m_missiles[ti];
        if (tgt.type != MissileType::Target || tgt.state != MissileState::Flying) continue;

        // Total-fire cap: don't keep wasting interceptors on hopeless targets
        int totalFired = m_totalFired.value(tgt.id, 0);
        if (totalFired >= MAX_INT_TOTAL) continue;

        // Count interceptors currently flying toward this target
        int flying = 0;
        for (int mi = 0; mi < n; ++mi) {
            const Missile& m = m_missiles[mi];
            if (m.type == MissileType::Interceptor && m.state == MissileState::Flying
                && m.targetId == tgt.id)
                ++flying;
        }
        if (flying >= MAX_INT_FLYING) continue;

        // Networked detection: any battery within range qualifies
        bool detected = false;
        for (int bi = 0; bi < m_batteries.size(); ++bi) {
            if ((tgt.pos - m_batteries[bi].pos).length() <= RADAR_RANGE) {
                detected = true; break;
            }
        }
        if (!detected) continue;

        // Rank all batteries by T_go, filtered by altitude band and ammo
        int   best1 = -1, best2 = -1;
        float tgo1  = 1e9f, tgo2 = 1e9f;

        for (int bi = 0; bi < m_batteries.size(); ++bi) {
            const LaunchBattery& b = m_batteries[bi];
            if (b.ammo <= 0 || b.reloadTimer > 0.f) continue;
            float alt = tgt.pos.z();
            if (alt < b.minAlt || alt > b.maxAlt) continue;

            float T_go = 0.f;
            Interceptor::predictIntercept(b.pos, b.intSpeed * 0.80f,
                                          tgt.pos, tgt.vel, &T_go);
            if (T_go < REACTION_TIME) continue;

            if (T_go < tgo1) {
                tgo2 = tgo1; best2 = best1;
                tgo1 = T_go; best1 = bi;
            } else if (bi != best1 && T_go < tgo2) {
                tgo2 = T_go; best2 = bi;
            }
        }

        if (best1 < 0) continue;

        // Log radar acquisition once per target
        if (!m_radarAcquired.contains(tgt.id)) {
            m_radarAcquired.insert(tgt.id);
            float rangekm = (tgt.pos - m_batteries[best1].pos).length() / 1000.f;
            emit eventLogged(
                QString("RADAR: Target #%1 acquired  range=%2 km  T_go≈%3 s")
                .arg(tgt.id).arg(rangekm, 0,'f',1).arg(tgo1, 0,'f',1));
        }

        // Queue interceptors to bring flying count up to MAX_INT_FLYING
        int needed = MAX_INT_FLYING - flying;
        if (needed >= 1 && best1 >= 0 && totalFired < MAX_INT_TOTAL)
            orders.append({tgt.id, best1});
        if (needed >= 2 && best2 >= 0 && totalFired + 1 < MAX_INT_TOTAL)
            orders.append({tgt.id, best2});
    }

    for (const auto& o : orders) {
        m_totalFired[o.targetId]++;
        launchInterceptorAt(o.targetId, o.batteryIdx);
    }
}

void Simulation::checkInterceptions()
{
    for (auto& intm : m_missiles) {
        if (intm.type != MissileType::Interceptor || intm.state != MissileState::Flying)
            continue;
        for (auto& tgt : m_missiles) {
            if (tgt.type != MissileType::Target || tgt.state != MissileState::Flying)
                continue;

            // CCD: minimum distance along path segment this tick
            QVector3D relVel  = intm.vel - tgt.vel;
            QVector3D relPos  = intm.pos - tgt.pos;          // end of step
            QVector3D relPosS = relPos - relVel * DT;        // approx start

            QVector3D D     = relVel * DT;
            float     dLen2 = D.lengthSquared();
            float     minDist;

            if (dLen2 > 0.f) {
                float s = -QVector3D::dotProduct(relPosS, D) / dLen2;
                s = qBound(0.f, s, 1.f);
                minDist = (relPosS + s * D).length();
            } else {
                minDist = relPos.length();
            }

            if (minDist <= intm.killRadius) {
                tgt.state  = MissileState::Intercepted;
                intm.state = MissileState::Exploded;
                QVector3D blastPos = (tgt.pos + intm.pos) * 0.5f;
                addExplosion(blastPos);
                if (m_game) m_game->onIntercept();
                emit eventLogged(
                    QString("INTERCEPT: #%1 killed %2 #%3  alt=%4 km  miss=%5 m")
                    .arg(intm.id)
                    .arg(getThreatSpec(tgt.threat).name).arg(tgt.id)
                    .arg(blastPos.z() / 1000.f, 0, 'f', 1)
                    .arg(minDist, 0, 'f', 0));
            }
        }
    }
}

void Simulation::updateManeuver(Missile& m, float dt)
{
    // Only maneuver during descent below 100 km
    if (m.pos.z() > 100000.f || m.vel.z() >= 0.f) {
        m.maneuverAccel = {};
        m.maneuverTimer = 0.f;
        return;
    }

    m.maneuverTimer -= dt;
    if (m.maneuverTimer <= 0.f) {
        // Pick a new random lateral direction perpendicular to velocity
        float angle = float(rand()) * 2.f * float(M_PI) / float(RAND_MAX);
        QVector3D lateral(qCos(angle), qSin(angle), 0.f);
        QVector3D velHat = m.vel.normalized();
        lateral -= QVector3D::dotProduct(lateral, velHat) * velHat;
        if (lateral.length() > 0.01f)
            m.maneuverAccel = lateral.normalized() * 35.f;  // ~3.5g
        m.maneuverTimer = 2.f + float(rand() % 400) / 100.f;  // 2-6 s
    }

    m.vel += m.maneuverAccel * dt;
}

void Simulation::addExplosion(QVector3D pos)
{
    Explosion ex;
    ex.pos      = pos;
    ex.duration = 0.5f;
    m_explosions.push_back(ex);
}
