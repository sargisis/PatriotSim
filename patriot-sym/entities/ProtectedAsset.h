#pragma once
#include <QVector3D>
#include <QString>

struct ProtectedAsset {
    enum Type { CITY = 0, AIRBASE = 1, RADAR = 2 };

    QVector3D pos;
    QString   name;
    Type      type;
    float     health     = 100.f;   // 0–100
    float     maxHealth  = 100.f;
    float     killRadius = 2000.f;  // m — missile must land within to deal damage
    float     damage     = 35.f;    // health lost per hit

    bool destroyed() const { return health <= 0.f; }
    float healthFrac() const { return health / maxHealth; }
};
