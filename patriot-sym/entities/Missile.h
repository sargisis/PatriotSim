#pragma once
#include <QVector3D>
#include <QVector>

enum class MissileType  { Target, Interceptor };
enum class MissileState { Flying, Intercepted, Missed, Exploded };

struct Missile {
    int           id       = -1;
    MissileType   type     = MissileType::Target;
    MissileState  state    = MissileState::Flying;
    QVector3D     pos;        // meters, ENU (East-North-Up)
    QVector3D     vel;        // m/s
    float         mass     = 800.f;   // kg
    float         diameter = 0.88f;  // m
    float         cd0      = 0.3f;
    float         age      = 0.f;    // seconds since launch
    int           targetId = -1;     // for interceptors
    QVector<QVector3D> trail;

    // Terminal-phase evasion (targets only)
    bool      maneuvering    = false;
    float     maneuverTimer  = 0.f;
    QVector3D maneuverAccel  = {};
};

struct Explosion {
    QVector3D pos;
    float age      = 0.f;
    float duration = 0.5f;
    bool  active   = true;
};
