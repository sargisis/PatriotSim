#pragma once
#include <QVector3D>

struct BallisticParams {
    float mass      = 800.f;   // kg
    float diameter  = 0.88f;   // m
    float cd0       = 0.3f;
    float latitude  = 40.18f;  // degrees (Yerevan)
    float windSpeed = 0.f;     // m/s
    float windDir   = 270.f;   // degrees from North, clockwise

    QVector3D windVec() const;
};

namespace Ballistic {
    float airDensity(float altM);
    float speedOfSound(float altM);
    float dragCoeff(float mach, float cd0);

    QVector3D acceleration(const QVector3D& pos, const QVector3D& vel,
                           const BallisticParams& p);

    void stepRK4(QVector3D& pos, QVector3D& vel, float dt,
                 const BallisticParams& p);
}
