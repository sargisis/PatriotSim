#include "Ballistic.h"
#include <QtMath>

static constexpr float G     = 9.80665f;
static constexpr float OMEGA = 7.2921e-5f;  // Earth rotation rad/s
static constexpr float PI    = 3.14159265f;

QVector3D BallisticParams::windVec() const
{
    float rad = windDir * PI / 180.f;
    // Wind blows FROM windDir, so velocity is opposite
    return QVector3D(-windSpeed * qSin(rad),
                     -windSpeed * qCos(rad),
                     0.f);
}

float Ballistic::airDensity(float altM)
{
    if (altM < 0) altM = 0;
    return 1.225f * qExp(-altM / 8500.f);
}

float Ballistic::speedOfSound(float altM)
{
    float factor = 1.f - 0.0000226f * altM;
    if (factor < 0.f) factor = 0.f;
    return 340.29f * qSqrt(qPow(factor, 5.256f));
}

float Ballistic::dragCoeff(float mach, float cd0)
{
    return cd0 + 0.5f * cd0 * qExp(-0.5f * (mach - 1.f) * (mach - 1.f));
}

QVector3D Ballistic::acceleration(const QVector3D& pos, const QVector3D& vel,
                                   const BallisticParams& p)
{
    const float altM = pos.z();
    const QVector3D vRel = vel - p.windVec();
    const float vRelMag  = vRel.length();

    // Gravity
    QVector3D a(0, 0, -G);

    // Aerodynamic drag
    if (vRelMag > 0.01f) {
        const float rho  = airDensity(altM);
        const float c    = speedOfSound(altM);
        const float mach = vRelMag / (c > 1.f ? c : 1.f);
        const float Cd   = dragCoeff(mach, p.cd0);
        const float A    = PI * (p.diameter * 0.5f) * (p.diameter * 0.5f);
        const float fd   = 0.5f * rho * vRelMag * vRelMag * Cd * A;
        a += -(fd / p.mass) * vRel.normalized();
    }

    // Coriolis: a_c = -2 * Omega × vel
    float latRad = p.latitude * PI / 180.f;
    QVector3D omegaVec(0.f, OMEGA * qCos(latRad), OMEGA * qSin(latRad));
    a += -2.f * QVector3D::crossProduct(omegaVec, vel);

    return a;
}

void Ballistic::stepRK4(QVector3D& pos, QVector3D& vel, float dt,
                         const BallisticParams& p)
{
    auto deriv = [&](const QVector3D& p0, const QVector3D& v0) {
        return std::make_pair(v0, acceleration(p0, v0, p));
    };

    auto [dp1, dv1] = deriv(pos, vel);
    auto [dp2, dv2] = deriv(pos + dp1*(dt/2), vel + dv1*(dt/2));
    auto [dp3, dv3] = deriv(pos + dp2*(dt/2), vel + dv2*(dt/2));
    auto [dp4, dv4] = deriv(pos + dp3*dt,      vel + dv3*dt);

    pos += (dt / 6.f) * (dp1 + 2*dp2 + 2*dp3 + dp4);
    vel += (dt / 6.f) * (dv1 + 2*dv2 + 2*dv3 + dv4);
}
