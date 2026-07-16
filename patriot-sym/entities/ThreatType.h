#pragma once

enum class ThreatType {
    SCUD_B,       // medium-range ballistic, 1500 m/s
    HWASONG,      // long-range ballistic, 2800 m/s
    CRUISE,       // low-altitude cruise, 280 m/s, launches from 80 km
    HYPERSONIC,   // 3500 m/s, maneuvering throughout entire flight
    MIRV          // ballistic, splits into 3 sub-warheads at apex
};

enum class WeaponSystem {
    PAC_3,        // engagement 5–40 km altitude, 2500 m/s, kill radius 75 m
    THAAD,        // engagement 40–150 km altitude, 3000 m/s, kill radius 150 m
    IRON_DOME     // engagement 0–10 km altitude, 900 m/s, kill radius 10 m
};

struct ThreatSpec {
    ThreatType  type;
    const char* name;
    float       speed;       // m/s
    float       elevation;   // degrees above horizon
    float       mass;        // kg
    float       diameter;    // m
    float       cd0;
    float       launchDist;  // km from origin
    bool        maneuvering; // maneuver throughout (not just terminal)
};

inline const ThreatSpec& getThreatSpec(ThreatType t)
{
    static const ThreatSpec specs[] = {
        { ThreatType::SCUD_B,     "SCUD-B",     1500, 65, 985,  0.88, 0.30, 250, false },
        { ThreatType::HWASONG,    "HWASONG-15", 2800, 45, 2000, 1.80, 0.25, 250, false },
        { ThreatType::CRUISE,     "CRUISE-M",    280,  4, 690,  0.51, 0.20,  80, false },
        { ThreatType::HYPERSONIC, "HYPERSONIC", 3500, 20, 4000, 0.50, 0.15, 250, true  },
        { ThreatType::MIRV,       "MIRV",       2500, 50, 1500, 1.00, 0.28, 250, false },
    };
    return specs[int(t)];
}
