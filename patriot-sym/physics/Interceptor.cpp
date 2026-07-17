#include "Interceptor.h"
#include <QtMath>
#include <algorithm>

static constexpr float G = 9.80665f;

QVector3D Interceptor::pnGuidance(const QVector3D& intPos, const QVector3D& intVel,
                                   const QVector3D& tgtPos, const QVector3D& tgtVel,
                                   float N)
{
    QVector3D r = tgtPos - intPos;
    float rMag  = r.length();
    if (rMag < 1.f) return {};

    QVector3D vRel = tgtVel - intVel;
    float Vc = -QVector3D::dotProduct(r, vRel) / rMag;

    // ω_LOS = (r × vRel) / |r|²  — axis of LOS rotation
    QVector3D omegaLOS = QVector3D::crossProduct(r, vRel) / (rMag * rMag);

    // Correct 3-D PN: a_cmd = N·Vc·(ω_LOS × r̂)
    // This lies IN the pursuit plane, perpendicular to the LOS — the right direction.
    // (The previous "N·Vc·ω_LOS" pointed OUT of the plane — completely wrong.)
    QVector3D rHat = r / rMag;
    return N * Vc * QVector3D::crossProduct(omegaLOS, rHat);
}

QVector3D Interceptor::acceleration(const QVector3D& intPos, const QVector3D& intVel,
                                     const QVector3D& tgtPos, const QVector3D& tgtVel,
                                     float age,
                                     float boostDuration, float boostAccel,
                                     float maxLatAccel, float N)
{
    QVector3D a(0, 0, -G);

    // Boost thrust along current velocity during first few seconds
    if (age < boostDuration && intVel.length() > 1.f)
        a += intVel.normalized() * boostAccel;

    // PN lateral guidance
    QVector3D aLat = pnGuidance(intPos, intVel, tgtPos, tgtVel, N);

    // Remove along-velocity component (keep lateral only)
    if (intVel.length() > 1.f) {
        QVector3D vHat = intVel.normalized();
        aLat -= QVector3D::dotProduct(aLat, vHat) * vHat;
    }

    // Clamp lateral acceleration
    float latMag = aLat.length();
    if (latMag > maxLatAccel && latMag > 0.f)
        aLat *= maxLatAccel / latMag;

    a += aLat;
    return a;
}

void Interceptor::stepRK4(QVector3D& pos, QVector3D& vel, float dt,
                            const QVector3D& tgtPos, const QVector3D& tgtVel,
                            float age, float boostDuration, float boostAccel,
                            float maxLatAccel)
{
    auto deriv = [&](const QVector3D& p0, const QVector3D& v0, float a0) {
        return std::make_pair(v0,
            acceleration(p0, v0, tgtPos, tgtVel, a0,
                         boostDuration, boostAccel, maxLatAccel));
    };

    auto [dp1, dv1] = deriv(pos, vel, age);
    auto [dp2, dv2] = deriv(pos+dp1*(dt/2), vel+dv1*(dt/2), age+dt/2);
    auto [dp3, dv3] = deriv(pos+dp2*(dt/2), vel+dv2*(dt/2), age+dt/2);
    auto [dp4, dv4] = deriv(pos+dp3*dt,      vel+dv3*dt,      age+dt);

    pos += (dt/6.f) * (dp1 + 2*dp2 + 2*dp3 + dp4);
    vel += (dt/6.f) * (dv1 + 2*dv2 + 2*dv3 + dv4);
}

QVector3D Interceptor::predictIntercept(const QVector3D& batteryPos,
                                         float interceptorSpeed,
                                         const QVector3D& tgtPos,
                                         const QVector3D& tgtVel,
                                         float* T_go)
{
    // Clamp to target's estimated time-to-ground so we never aim below ground
    float timeToGround = 1000.f;
    if (tgtPos.z() > 10.f && tgtVel.z() < -0.1f)
        timeToGround = -tgtPos.z() / tgtVel.z();

    // Bisection: find T where interceptor (at speed S from batteryPos)
    // can reach the linearly-extrapolated target position
    float tLo = 0.f;
    float tHi = std::min(timeToGround * 0.95f, 120.f);

    // Quick sanity: can we even reach the target at tHi?
    // If not, clamp to direct pursuit
    {
        QVector3D futurePos = tgtPos + tgtVel * tHi;
        float dist = (futurePos - batteryPos).length();
        if (dist > interceptorSpeed * tHi) {
            // Unreachable within window — aim at current position
            if (T_go) *T_go = std::max(tHi * 0.5f, 1.f);
            return tgtPos + tgtVel * std::max(tHi * 0.5f, 1.f);
        }
    }

    // 24 iterations of bisection (sub-metre accuracy)
    for (int i = 0; i < 24; ++i) {
        float tMid  = (tLo + tHi) * 0.5f;
        QVector3D p = tgtPos + tgtVel * tMid;
        float distToP = (p - batteryPos).length();
        if (distToP < interceptorSpeed * tMid)
            tHi = tMid;   // interceptor arrives before target → close window
        else
            tLo = tMid;   // interceptor can't reach yet → open window
    }

    float T = (tLo + tHi) * 0.5f;
    T = std::max(T, 0.5f);

    if (T_go) *T_go = T;
    return tgtPos + tgtVel * T;
}
