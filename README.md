# PatriotSim — PAC-3 Ballistic Missile Intercept Simulator

> A real-time, physics-accurate air defense simulator built with Qt6 and OpenGL 3.3

![C++](https://img.shields.io/badge/C++20-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Qt](https://img.shields.io/badge/Qt6-41CD52?style=flat&logo=qt&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL_3.3-5586A4?style=flat&logo=opengl&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D6?style=flat&logo=windows&logoColor=white)

---

## Overview

PatriotSim models a multi-battery PAC-3 Patriot air defense system defending against ballistic missile threats. Every missile — target and interceptor alike — follows a full physics simulation: atmospheric drag, gravity, Coriolis effect, and transonic/supersonic drag transitions. Interceptors are guided by a corrected 3D Proportional Navigation law and hunt targets autonomously.

The UI is styled as a military command-and-control console — dark theme, docked panels, a live radar scope, and an event log with color-coded threat assessment.

---

## Features

### Physics Engine
- **RK4 integration** — 4th-order Runge-Kutta for smooth, accurate trajectories at any simulation speed
- **ISA atmosphere** — density ρ(h) = 1.225·exp(−h/8500), speed of sound c(h) from standard lapse rate
- **Mach-dependent drag** — Cd(M) = Cd₀ + 0.5·Cd₀·exp(−0.5·(M−1)²) with transonic spike
- **Coriolis effect** — a_c = −2Ω×v in ENU frame at configurable latitude
- **Wind** — configurable speed and direction applied to all aerodynamic calculations

### Guidance & Interception
- **3D Proportional Navigation** — corrected formula `a = N·Vc·(ω_LOS × r̂)` — acceleration lies in the pursuit plane
- **Bisection intercept prediction** — 24-iteration solver finds intercept point, clamped to target's time-to-ground
- **Continuous Collision Detection** — minimum-distance along path segment per tick; no misses at high closing speeds
- **Salvo engagement** — two interceptors per target from different batteries for shoot-look-shoot

### Air Defense System
- **3 PAC-3 batteries** in triangular formation (BTY-A, BTY-B, BTY-C)
- **Networked radar** — shared AN/MPQ-65 fire control; any battery can engage any detected target
- **Limited ammo** — 8 interceptors per battery; WINCHESTER warning on depletion; replenished on reset
- **Auto-intercept** — threat detection, target prioritization, and launch sequencing run automatically
- **Total engagement cap** — maximum 4 interceptors per target to prevent salvo spam

### Threat Simulation
- Configurable **speed** (500–3000 m/s), **elevation** (20–85°), **azimuth**, **mass**, and **diameter**
- **Maneuvering targets** — 3.5g lateral evasion maneuvers in terminal phase below 100 km altitude
- **Click-to-aim** — click anywhere on the 3D view to direct a target at that ground coordinate

### Visualization
- **3D OpenGL scene** — ground grid, missile trails with per-vertex alpha fade, explosion effects
- **AN/MPQ-65 radar scope** — real-time PPI display with rotating sweep, range rings, hostile/friendly blips, and battery status
- **HUD overlay** — telemetry readout, compass rose, tracking reticles, battery labels
- **Military C2 UI** — QDockWidget layout, QSS dark theme, color-coded event log

---

## Building

### Requirements

| Tool | Version |
|------|---------|
| Qt | 6.11+ (Widgets, OpenGL, OpenGLWidgets) |
| CMake | 3.20+ |
| Compiler | MinGW 13.1 / MSVC 2022 / GCC 13 (C++20) |

### Steps

```bash
git clone https://github.com/sargisis/PatriotSim.git
cd PatriotSim/patriot-sym

cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build build --config Release
```

Run the executable from the `build/` directory with Qt DLLs in PATH, or use `windeployqt`.

---

## Controls

| Input | Action |
|-------|--------|
| **Left drag** | Orbit camera |
| **Right drag** | Pan camera |
| **Scroll wheel** | Zoom in/out |
| **LMB click** *(click-to-aim on)* | Launch target at clicked point |
| **RESET** | Clear all missiles, restore ammo |
| **PAUSE / RESUME** | Freeze/unfreeze simulation |
| **×1 / ×2 / ×4** | Simulation speed multiplier |

---

## Architecture

```
PatriotSim/
└── patriot-sym/
    ├── entities/
    │   └── Missile.h            # Missile & Explosion data structs
    ├── physics/
    │   ├── Ballistic.h/cpp      # RK4 + ISA atmosphere + drag + Coriolis
    │   └── Interceptor.h/cpp    # PN guidance + bisection intercept prediction
    ├── Simulation.h/cpp         # Game loop, radar detection, engagement logic
    ├── SimRenderer.h/cpp        # OpenGL 3.3 scene + QPainter HUD overlay
    ├── RadarDisplay.h/cpp       # AN/MPQ-65 PPI radar scope widget
    ├── mainwindow.h/cpp         # Qt dock UI, parameter controls, event log
    ├── theme.qss                # Military dark QSS stylesheet
    └── CMakeLists.txt
```

---

## Physics Reference

**Atmospheric model (ISA)**
```
ρ(h) = 1.225 · exp(−h / 8500)                       [kg/m³]
c(h) = 340.29 · √((1 − 2.26×10⁻⁵·h)^5.256)         [m/s]
```

**Aerodynamic drag**
```
Cd(M) = Cd₀ + 0.5·Cd₀·exp(−0.5·(M − 1)²)
F_drag = −0.5 · ρ · Cd · A · |v| · v
```

**Coriolis acceleration**
```
Ω = ω_Earth · (0, cos φ, sin φ)    [ENU frame]
a_c = −2Ω × v
```

**Proportional Navigation (3D)**
```
r̂      = (target_pos − interceptor_pos) / |...|
ω_LOS  = (r × v_rel) / |r|²
a_cmd  = N · Vc · (ω_LOS × r̂)      [N = 5,  Vc = closing speed]
```

---

## Roadmap

- [ ] Protected assets — city/base objects with integrity, mission-failure condition
- [ ] Wave-based attack scenarios with increasing saturation
- [ ] Kill statistics HUD — intercept %, ammo efficiency, mission score
- [ ] Multiple threat types — SCUD-B, cruise missiles, hypersonic glide vehicles
- [ ] Decoy warheads — MIRVs and radar-spoofing penetration aids
- [ ] Battery reload with realistic cooldown timer
- [ ] Multi-layer defense — THAAD upper tier + Iron Dome lower tier
- [ ] Pre-mission battery placement editor
- [ ] Scenario campaign with resource management between missions

---

## License

MIT — free to use, modify, and distribute.

---

<div align="center">
  <sub>Built with Qt6 · OpenGL 3.3 · C++20</sub>
</div>
