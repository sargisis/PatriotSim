#pragma once
#include <QVector3D>

namespace Interceptor {

// Returns lateral acceleration command (m/s²) via Proportional Navigation
// N=4, V_c = closing speed, omega_LOS = LOS angular rate vector
QVector3D pnGuidance(const QVector3D& intPos, const QVector3D& intVel,
                     const QVector3D& tgtPos, const QVector3D& tgtVel,
                     float N = 5.f);

// Full interceptor acceleration: PN + gravity + thrust along vel
QVector3D acceleration(const QVector3D& intPos, const QVector3D& intVel,
                        const QVector3D& tgtPos, const QVector3D& tgtVel,
                        float age,
                        float boostDuration = 3.f,   // seconds
                        float boostAccel    = 150.f, // m/s²
                        float maxLatAccel   = 400.f, // m/s² (40g)
                        float N             = 5.f);

void stepRK4(QVector3D& pos, QVector3D& vel, float dt,
             const QVector3D& tgtPos, const QVector3D& tgtVel,
             float age,
             float boostDuration = 4.f,
             float boostAccel    = 250.f,
             float maxLatAccel   = 300.f);

// Predict intercept point assuming linear target motion and interceptor at speed
// Returns predicted intercept position, sets T_go
QVector3D predictIntercept(const QVector3D& batteryPos,
                            float interceptorSpeed,
                            const QVector3D& tgtPos,
                            const QVector3D& tgtVel,
                            float* T_go = nullptr);

}
