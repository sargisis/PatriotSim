#include "Simulation.h"
#include "physics/Ballistic.h"
#include "physics/Interceptor.h"
#include <QtMath>
#include <QSet>

static constexpr float PI = 3.14159265f;

Simulation::Simulation(QObject* parent) : QObject(parent)
{
    // Triangle of three PAC-3 batteries — different angles, better coverage
    m_batteries = {
        {{ 0,      10000, 0}, "BTY-A", 8, 8},
        {{ 12000, -6000, 0}, "BTY-B", 8, 8},
        {{-12000, -6000, 0}, "BTY-C", 8, 8},
    };

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

void Simulation::launchTarget(QVector3D impactHint)
{
    Missile m;
    m.id       = m_nextId++;
    m.type     = MissileType::Target;
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

    const Missile* tgt = nullptr;
    for (const auto& m : m_missiles)
        if (m.id == targetId && m.state == MissileState::Flying) { tgt = &m; break; }
    if (!tgt) return;

    const QVector3D& bPos = bty.pos;

    float T_go = 0.f;
    // Conservative speed estimate (gravity/drag reduce average speed)
    QVector3D ip = Interceptor::predictIntercept(bPos, INT_SPEED * 0.80f,
                                                  tgt->pos, tgt->vel, &T_go);

    // Don't fire if T_go is too short to intercept usefully
    if (T_go < REACTION_TIME) return;

    Missile intm;
    intm.id       = m_nextId++;
    intm.type     = MissileType::Interceptor;
    intm.mass     = 90.f;
    intm.diameter = 0.255f;
    intm.cd0      = 0.1f;
    intm.pos      = bPos;
    intm.targetId = targetId;
    intm.vel      = (ip - bPos).normalized() * INT_SPEED;
    m_missiles.push_back(intm);
    bty.ammo--;

    emit eventLogged(QString("PAC-3 %1 [%2]: Interceptor #%3 → TGT #%4  T_go=%5 s  alt=%6 km")
                     .arg(bty.name).arg(bty.ammo)
                     .arg(intm.id).arg(targetId)
                     .arg(T_go, 0, 'f', 1)
                     .arg(ip.z() / 1000.f, 0, 'f', 1));
    emit ammoBatteryUpdated(batteryIdx, bty.ammo, bty.maxAmmo);
}

void Simulation::reset()
{
    m_missiles.clear();
    m_explosions.clear();
    m_radarAcquired.clear();
    m_totalFired.clear();
    for (auto& b : m_batteries) b.ammo = b.maxAmmo;
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

    if (m_autoIntercept)
        detectAndAutoLaunch();
    checkInterceptions();

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
            emit eventLogged(QString("IMPACT: Target #%1 — ground strike  (not intercepted)").arg(m.id));
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

        // Rank ALL batteries by T_go — no per-battery range restriction
        // (networked fire control — any battery can engage any detected target)
        int   best1 = -1, best2 = -1;
        float tgo1  = 1e9f, tgo2 = 1e9f;

        for (int bi = 0; bi < m_batteries.size(); ++bi) {
            float T_go = 0.f;
            Interceptor::predictIntercept(m_batteries[bi].pos, INT_SPEED * 0.80f,
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

            if (minDist <= KILL_RADIUS) {
                tgt.state  = MissileState::Intercepted;
                intm.state = MissileState::Exploded;
                QVector3D blastPos = (tgt.pos + intm.pos) * 0.5f;
                addExplosion(blastPos);
                emit eventLogged(
                    QString("INTERCEPT: #%1 destroyed TGT #%2  alt=%3 km  miss=%4 m")
                    .arg(intm.id).arg(tgt.id)
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
