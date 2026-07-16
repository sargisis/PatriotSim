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
        {{ 0,     10000, 0}, "THAAD-1", WeaponSystem::THAAD,
          8,  8, 3000.f, 150.f, 40000.f, 150000.f, 0.f},
        {{ 12000, -6000, 0}, "PAC3-A",  WeaponSystem::PAC_3,
          8,  8, 2500.f,  75.f,  5000.f,  40000.f, 0.f},
        {{-12000, -6000, 0}, "PAC3-B",  WeaponSystem::PAC_3,
          8,  8, 2500.f,  75.f,  5000.f,  40000.f, 0.f},
        {{ 4000,   4000, 0}, "IDOME-1", WeaponSystem::IRON_DOME,
          24, 24,  900.f,   8.f,    50.f,  10000.f, 0.f},
    };

    m_game = new GameState(this);
    connect(m_game, &GameState::launchRequested,
            this,   &Simulation::onGameLaunchRequested);

    m_ai = new EngagementAI();
    m_ai->load("patriot_ai.bin");  // load prior training if exists

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

    // Start reload when last interceptor is fired
    if (bty.ammo == 0) {
        float reloadTime = (bty.wsys == WeaponSystem::THAAD)     ? 120.f
                         : (bty.wsys == WeaponSystem::IRON_DOME) ?  20.f
                                                                  :  45.f;
        bty.reloadTimer = reloadTime;
        emit eventLogged(QString("%1: WINCHESTER — reloading in %2 s")
                         .arg(bty.name).arg(int(reloadTime)));
    }

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
                Missile sub;
                sub.id         = m_nextId++;
                sub.type       = MissileType::Target;
                sub.threat     = ThreatType::SCUD_B;
                sub.mass       = 300.f;
                sub.diameter   = 0.65f;
                sub.cd0        = 0.28f;
                sub.maneuvering= false;
                sub.pos        = m.pos;
                sub.vel        = m.vel * 0.95f + perpDir * float(i) * 200.f;
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
    if (m_ai)  { m_ai->save("patriot_ai.bin"); }
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
            bool hitAsset = false;
            if (m_game) {
                m_game->onGroundImpact(m.pos);
                for (const auto& a : m_game->assets())
                    if ((QVector3D(m.pos.x(),m.pos.y(),0)-QVector3D(a.pos.x(),a.pos.y(),0)).length() <= a.killRadius)
                    { hitAsset = true; break; }
            }
            if (m_ai) m_ai->onGroundImpact(m.id, hitAsset);
            emit eventLogged(QString("IMPACT: %1 #%2 — ground strike")
                             .arg(getThreatSpec(m.threat).name).arg(m.id));
        } else {
            m.state = MissileState::Missed;
        }
    }
}

AIEngagementState Simulation::buildAIState(const Missile& tgt, int flyingCount) const
{
    AIEngagementState s{};
    s.altNorm     = tgt.pos.z() / 150000.f;
    s.speedNorm   = tgt.vel.length() / 4000.f;
    s.threatNorm  = float(int(tgt.threat)) / 4.f;
    s.maneuvering = tgt.maneuvering ? 1.f : 0.f;

    float minDistAsset = 1e9f;
    if (m_game) {
        for (const auto& a : m_game->assets()) {
            if (a.destroyed()) continue;
            float d = QVector3D(tgt.pos.x()-a.pos.x(), tgt.pos.y()-a.pos.y(), 0).length();
            minDistAsset = qMin(minDistAsset, d);
        }
    }
    s.assetThreat = (minDistAsset < 1e8f) ? qBound(0.f, 1.f - minDistAsset/20000.f, 1.f) : 0.f;

    float posLen = tgt.pos.length();
    if (posLen > 0.1f) {
        QVector3D posHat = tgt.pos / posLen;
        QVector3D velHat = tgt.vel.normalized();
        s.headingInward = qBound(0.f, -QVector3D::dotProduct(velHat, posHat), 1.f);
    }

    for (int i = 0; i < qMin(4, (int)m_batteries.size()); ++i) {
        const LaunchBattery& b = m_batteries[i];
        float alt = tgt.pos.z();
        bool inBand  = (alt >= b.minAlt && alt <= b.maxAlt);
        bool canFire = (b.ammo > 0 && b.reloadTimer <= 0.f && inBand);
        float T_go = 0.f;
        if (canFire)
            Interceptor::predictIntercept(b.pos, b.intSpeed * 0.80f,
                                          tgt.pos, tgt.vel, &T_go);
        if (canFire && T_go < REACTION_TIME) canFire = false;
        s.eligible[i] = canFire ? 1.f : 0.f;
        s.tgoNorm[i]  = (canFire && T_go > 0.f) ? qBound(0.f, T_go/60.f, 1.f) : 0.f;
        s.ammoNorm[i] = float(b.ammo) / float(qMax(1, b.maxAmmo));
    }
    return s;
}

void Simulation::detectAndAutoLaunch()
{
    struct Order { int targetId; int batteryIdx; };
    QVector<Order> orders;

    // Networked radar: target is "seen" if within RADAR_RANGE of ANY battery.
    // (Real Patriot uses a shared AN/MPQ-65 radar — all batteries share the picture.)
    const int n = m_missiles.size();

    for (int ti = 0; ti < n; ++ti) {
        const Missile& tgt = m_missiles[ti];
        if (tgt.type != MissileType::Target || tgt.state != MissileState::Flying) continue;

        int totalFired = m_totalFired.value(tgt.id, 0);
        if (totalFired >= MAX_INT_TOTAL) continue;

        int flying = 0;
        for (int mi = 0; mi < n; ++mi) {
            const Missile& m2 = m_missiles[mi];
            if (m2.type == MissileType::Interceptor && m2.state == MissileState::Flying
                && m2.targetId == tgt.id)
                ++flying;
        }
        if (flying >= MAX_INT_FLYING) continue;

        // Networked radar check
        bool detected = false;
        for (int bi = 0; bi < m_batteries.size(); ++bi)
            if ((tgt.pos - m_batteries[bi].pos).length() <= RADAR_RANGE)
            { detected = true; break; }
        if (!detected) continue;

        // Log acquisition once
        if (!m_radarAcquired.contains(tgt.id)) {
            m_radarAcquired.insert(tgt.id);
            emit eventLogged(
                QString("RADAR: %1 #%2  alt=%3 km  v=%4 m/s")
                .arg(getThreatSpec(tgt.threat).name).arg(tgt.id)
                .arg(tgt.pos.z()/1000.f,0,'f',1)
                .arg(tgt.vel.length(),0,'f',0));
        }

        // ── AI engagement decision ─────────────────────────────
        AIEngagementState aiState = buildAIState(tgt, flying);
        int batteryChoice = m_ai->decide(tgt.id, aiState);

        if (batteryChoice >= 0 && totalFired < MAX_INT_TOTAL) {
            orders.append({tgt.id, batteryChoice});
            // If AI chose to fire and we still need a second interceptor, ask again
            // (second decision uses a fresh state — the first battery's ammo is unchanged
            //  until after the loop, so AI can pick a different battery)
            if (flying + 1 < MAX_INT_FLYING && totalFired + 1 < MAX_INT_TOTAL) {
                // Temporarily mark chosen battery ineligible for second pick
                AIEngagementState aiState2 = aiState;
                if (batteryChoice < 4) aiState2.eligible[batteryChoice] = 0.f;
                int second = m_ai->decide(tgt.id + 10000, aiState2);  // synthetic id
                if (second >= 0 && second != batteryChoice)
                    orders.append({tgt.id, second});
            }
        }
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
                if (m_ai)  m_ai->onIntercept(tgt.id);
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
