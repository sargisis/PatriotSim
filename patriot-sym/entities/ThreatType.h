#pragma once

// ── Threat types ──────────────────────────────────────────────────
// Real-world missile data from open sources (CSIS, Wikipedia, Jane's)
enum class ThreatType {
    SCUD_B,       // R-17 Elbrus SRBM — 1,500 m/s, 300 km, purely ballistic
    KN23,         // KN-23 / Hwasong-11 — quasi-ballistic, pull-up maneuver, ~30G
    ISKANDER,     // Iskander-M 9M723 — quasi-ballistic, 30G throughout, Mach 6-7
    SHAHAB3,      // Shahab-3 MRBM — 1,400 m/s terminal, 1,300 km range, high arc
    CRUISE,       // Kalibr/Tomahawk-type — subsonic cruise, terrain-following
    SHAHED,       // Shahed-136 loitering munition — very slow, low-alt, tiny RCS
    HYPERSONIC,   // HGV (DF-17 type) — Mach 9+, glides 40-80 km alt, maneuvering
    MIRV          // MIRV bus — splits into 3 sub-warheads at apogee
};

// ── Defense systems ───────────────────────────────────────────────
enum class WeaponSystem {
    PAC_3,        // PAC-3 MSE: 1,700 m/s, 500m–24km, 12 rds/launcher, Pk~0.80
    THAAD,        // THAAD: 2,800 m/s, 40–150km, 8 rds/launcher, hit-to-kill, Pk~0.90
    IRON_DOME     // Iron Dome: 750 m/s, 100m–10km, 20 rds/launcher, Pk~0.90
};

// ── Threat specification ──────────────────────────────────────────
struct ThreatSpec {
    ThreatType  type;
    const char* name;
    float       speed;       // m/s — burnout or cruise speed
    float       elevation;   // degrees above horizon at launch
    float       mass;        // kg — full missile mass
    float       diameter;    // m — body diameter
    float       cd0;         // zero-lift drag coefficient
    float       launchDist;  // km from origin
    bool        maneuvering; // true = executes evasive maneuvers
    float       maxG;        // max lateral acceleration (m/s²) during maneuver
};

inline const ThreatSpec& getThreatSpec(ThreatType t)
{
    // Real data — sources: CSIS Missile Threat, Wikipedia, Jane's open summaries
    static const ThreatSpec specs[] = {
        // type           name           spd   el   mass     diam  cd0   dist  mnvr  maxG
        // ─── Ballistic missiles ────────────────────────────────────────────────────────
        { ThreatType::SCUD_B,    "SCUD-B",     1500, 65,   985, 0.885, 0.30,  250, false,   0 },
        // KN-23: shorter range, depressed trajectory (40° not 65°), 30G pull-up
        { ThreatType::KN23,      "KN-23",      2000, 40,  1800, 0.900, 0.22,  400, true,  294 },
        // Iskander-M: quasi-ballistic, low apogee (25° launch), extreme maneuver throughout
        { ThreatType::ISKANDER,  "ISKANDER-M", 2300, 25,  4615, 0.920, 0.18,  400, true,  294 },
        // Shahab-3: MRBM, high arc (80°), heavy, long range, no maneuver
        { ThreatType::SHAHAB3,   "SHAHAB-3",   1400, 78, 16000, 1.350, 0.28, 1000, false,   0 },
        // ─── Aerodynamic threats ───────────────────────────────────────────────────────
        // Kalibr/Kh-101 type: subsonic, low-level cruise
        { ThreatType::CRUISE,    "CRUISE-KAL",  280,  4,   690, 0.510, 0.20,   80, false,   0 },
        // Shahed-136: very slow loitering munition, tiny RCS, low altitude
        { ThreatType::SHAHED,    "SHAHED-136",  150,  1,   200, 0.250, 0.35,  100, false,   0 },
        // ─── Advanced threats ──────────────────────────────────────────────────────────
        // DF-17 HGV: Mach 9+, glides at 40-80 km, highly maneuverable
        { ThreatType::HYPERSONIC,"HGV-DF17",   3000, 20,  4000, 0.500, 0.15,  500, true,  196 },
        // MIRV bus: splits into 3 sub-warheads at apogee
        { ThreatType::MIRV,      "MIRV",       2500, 55,  1500, 1.000, 0.28,  250, false,   0 },
    };
    return specs[int(t)];
}
