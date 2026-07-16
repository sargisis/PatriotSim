#pragma once
#include <QVector3D>
#include <QVector>
#include "ThreatType.h"

enum class MissileType  { Target, Interceptor };
enum class MissileState { Flying, Intercepted, Missed, Exploded };

struct Missile {
    int           id       = -1;
    MissileType   type     = MissileType::Target;
    MissileState  state    = MissileState::Flying;
    ThreatType    threat   = ThreatType::SCUD_B;   // target threat type
    QVector3D     pos;
    QVector3D     vel;
    float         mass       = 800.f;
    float         diameter   = 0.88f;
    float         cd0        = 0.3f;
    float         age        = 0.f;
    float         killRadius = 75.f;   // interceptor kill radius (from battery type)
    int           targetId   = -1;
    int           launchBattery = -1;  // which battery launched this interceptor
    QVector<QVector3D> trail;

    // MIRV tracking
    bool wasAscending = true;    // last tick was ascending
    bool milvSplit    = false;   // already split (MIRV only)

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
