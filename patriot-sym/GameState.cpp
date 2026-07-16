#include "GameState.h"
#include <QtMath>
#include <algorithm>

// ── Scenario definitions ────────────────────────────────────────

static ScenarioDef buildDesertStorm()
{
    ScenarioDef s;
    s.name     = "DESERT STORM";
    s.briefing = "Intelligence reports incoming SCUD-B strikes. 3 waves, moderate density. Defend the capital.";

    // Wave 1 — 3 SCUD from south
    WaveDef w1;
    w1.prePauseSec = 10.f; w1.intervalSec = 5.f;
    for (int i = 0; i < 3; ++i) w1.threats.push_back({ ThreatType::SCUD_B, 0.f + i * 10.f });
    s.waves.push_back(w1);

    // Wave 2 — 5 SCUD from varying azimuths
    WaveDef w2;
    w2.prePauseSec = 12.f; w2.intervalSec = 4.f;
    for (float az : {340.f, 10.f, 20.f, 350.f, 5.f})
        w2.threats.push_back({ ThreatType::SCUD_B, az });
    s.waves.push_back(w2);

    // Wave 3 — 6 SCUD + 2 HWASONG
    WaveDef w3;
    w3.prePauseSec = 15.f; w3.intervalSec = 3.f;
    for (float az : {330.f, 340.f, 350.f, 0.f, 10.f, 20.f})
        w3.threats.push_back({ ThreatType::SCUD_B, az });
    w3.threats.push_back({ ThreatType::HWASONG, 5.f });
    w3.threats.push_back({ ThreatType::HWASONG, 355.f });
    s.waves.push_back(w3);

    // Assets
    s.assets = {
        { {0, 3000, 0}, "CAPITAL CITY",   ProtectedAsset::CITY,    100, 100, 3000, 30 },
        { {8000, 0, 0}, "AIR BASE",       ProtectedAsset::AIRBASE, 100, 100, 1500, 45 },
        { {0,-8000, 0}, "RADAR STATION",  ProtectedAsset::RADAR,   100, 100,  600, 60 },
    };
    return s;
}

static ScenarioDef buildKoreaCrisis()
{
    ScenarioDef s;
    s.name     = "KOREA CRISIS";
    s.briefing = "Mixed ballistic and MIRV threat package detected. 4 waves. Radar station is priority target.";

    // Wave 1
    WaveDef w1; w1.prePauseSec = 10.f; w1.intervalSec = 4.f;
    for (float az : {0.f, 15.f, 345.f, 30.f}) w1.threats.push_back({ ThreatType::SCUD_B, az });
    s.waves.push_back(w1);

    // Wave 2 — first MIRV + HWASONG
    WaveDef w2; w2.prePauseSec = 12.f; w2.intervalSec = 3.5f;
    w2.threats.push_back({ ThreatType::MIRV, 5.f });
    for (float az : {350.f, 10.f, 20.f}) w2.threats.push_back({ ThreatType::HWASONG, az });
    s.waves.push_back(w2);

    // Wave 3 — cruise + ballistic mix
    WaveDef w3; w3.prePauseSec = 12.f; w3.intervalSec = 3.f;
    for (float az : {90.f, 100.f, 80.f}) w3.threats.push_back({ ThreatType::CRUISE, az });
    for (float az : {0.f, 350.f, 10.f}) w3.threats.push_back({ ThreatType::SCUD_B, az });
    s.waves.push_back(w3);

    // Wave 4 — saturation
    WaveDef w4; w4.prePauseSec = 15.f; w4.intervalSec = 2.5f;
    w4.threats.push_back({ ThreatType::MIRV, 0.f });
    w4.threats.push_back({ ThreatType::MIRV, 20.f });
    for (float az : {340.f, 350.f, 0.f, 10.f, 20.f, 30.f})
        w4.threats.push_back({ ThreatType::HWASONG, az });
    s.waves.push_back(w4);

    s.assets = {
        { {0, 3000, 0}, "CAPITAL CITY",  ProtectedAsset::CITY,    100, 100, 3000, 25 },
        { {8000, 0, 0}, "AIR BASE",      ProtectedAsset::AIRBASE, 100, 100, 1500, 40 },
        { {0,-8000, 0}, "RADAR STATION", ProtectedAsset::RADAR,   100, 100,  600, 55 },
    };
    return s;
}

static ScenarioDef buildTotalSaturation()
{
    ScenarioDef s;
    s.name     = "TOTAL SATURATION";
    s.briefing = "All threat types inbound. 5 waves. Hypersonic glide vehicles confirmed. Good luck.";

    // Wave 1 — warm-up
    WaveDef w1; w1.prePauseSec = 8.f; w1.intervalSec = 3.f;
    for (float az : {0.f, 20.f, 340.f, 10.f, 350.f}) w1.threats.push_back({ ThreatType::SCUD_B, az });
    s.waves.push_back(w1);

    // Wave 2 — first hypersonic
    WaveDef w2; w2.prePauseSec = 10.f; w2.intervalSec = 2.5f;
    w2.threats.push_back({ ThreatType::HYPERSONIC, 5.f });
    for (float az : {345.f, 355.f, 5.f, 15.f}) w2.threats.push_back({ ThreatType::HWASONG, az });
    w2.threats.push_back({ ThreatType::MIRV, 0.f });
    s.waves.push_back(w2);

    // Wave 3 — cruise flanking attack
    WaveDef w3; w3.prePauseSec = 12.f; w3.intervalSec = 2.f;
    for (float az : {85.f, 95.f, 90.f, 270.f, 275.f, 265.f})
        w3.threats.push_back({ ThreatType::CRUISE, az });
    w3.threats.push_back({ ThreatType::HYPERSONIC, 355.f });
    s.waves.push_back(w3);

    // Wave 4 — multi-MIRV + hypersonic
    WaveDef w4; w4.prePauseSec = 12.f; w4.intervalSec = 2.f;
    for (int i = 0; i < 3; ++i) w4.threats.push_back({ ThreatType::MIRV, float(i*15) });
    for (int i = 0; i < 2; ++i) w4.threats.push_back({ ThreatType::HYPERSONIC, float(i*10+350) });
    for (float az : {0.f, 10.f, 350.f, 340.f}) w4.threats.push_back({ ThreatType::HWASONG, az });
    s.waves.push_back(w4);

    // Wave 5 — everything
    WaveDef w5; w5.prePauseSec = 15.f; w5.intervalSec = 1.5f;
    for (float az : {0.f, 30.f, 330.f, 10.f, 350.f, 20.f, 340.f, 5.f, 355.f, 15.f})
        w5.threats.push_back({ ThreatType::SCUD_B, az });
    for (int i = 0; i < 3; ++i) w5.threats.push_back({ ThreatType::HYPERSONIC, float(i*15) });
    for (int i = 0; i < 3; ++i) w5.threats.push_back({ ThreatType::MIRV, float(i*10+355) });
    s.waves.push_back(w5);

    s.assets = {
        { {0, 3000, 0}, "CAPITAL CITY",  ProtectedAsset::CITY,    100, 100, 3000, 20 },
        { {8000, 0, 0}, "AIR BASE",      ProtectedAsset::AIRBASE, 100, 100, 1500, 35 },
        { {0,-8000, 0}, "RADAR STATION", ProtectedAsset::RADAR,   100, 100,  600, 50 },
    };
    return s;
}

// ── GameState ────────────────────────────────────────────────────

GameState::GameState(QObject* parent) : QObject(parent) {}

QString GameState::scenarioName(int idx)
{
    switch (idx) {
    case 0: return "DESERT STORM";
    case 1: return "KOREA CRISIS";
    case 2: return "TOTAL SATURATION";
    default: return {};
    }
}

QString GameState::scenarioBriefing(int idx)
{
    switch (idx) {
    case 0: return buildDesertStorm().briefing;
    case 1: return buildKoreaCrisis().briefing;
    case 2: return buildTotalSaturation().briefing;
    default: return {};
    }
}

void GameState::buildScenario(int idx)
{
    ScenarioDef def;
    switch (idx) {
    case 0: def = buildDesertStorm();    break;
    case 1: def = buildKoreaCrisis();   break;
    default:def = buildTotalSaturation();break;
    }
    m_waves  = def.waves;
    m_assets = def.assets;
}

void GameState::startScenario(int idx)
{
    m_waveIdx    = -1;
    m_score      = 0;
    m_intercepted= 0;
    m_missed     = 0;
    m_launchIdx  = 0;
    m_launchTimer= 0.f;
    m_interWave  = 0.f;

    buildScenario(idx);
    setPhase(Phase::COUNTDOWN);
    beginWave(0);
}

void GameState::reset()
{
    m_waveIdx = -1;
    m_waves.clear();
    m_assets.clear();
    setPhase(Phase::IDLE);
}

void GameState::setPhase(Phase p)
{
    if (m_phase == p) return;
    m_phase = p;
    emit phaseChanged(p);
}

void GameState::beginWave(int idx)
{
    m_waveIdx     = idx;
    m_launchIdx   = 0;
    m_launchTimer = 0.f;
    m_countdown   = m_waves[idx].prePauseSec;
    setPhase(Phase::COUNTDOWN);
    emit waveStarted(idx + 1, m_waves.size());
    emit logEvent(QString("WAVE %1/%2 — INBOUND  (%3 threats)")
                  .arg(idx+1).arg(m_waves.size())
                  .arg(m_waves[idx].threats.size()));
}

bool GameState::allAssetsDestroyed() const
{
    for (const auto& a : m_assets) if (!a.destroyed()) return false;
    return !m_assets.isEmpty();
}

void GameState::tick(float dt, int flyingTargetCount)
{
    if (m_phase == Phase::IDLE ||
        m_phase == Phase::MISSION_WIN ||
        m_phase == Phase::MISSION_FAIL) return;

    // Check fail condition
    if (allAssetsDestroyed()) {
        setPhase(Phase::MISSION_FAIL);
        emit missionEnded(false, m_score, m_intercepted, m_missed);
        emit logEvent("MISSION FAILED — all protected assets destroyed");
        return;
    }

    if (m_phase == Phase::COUNTDOWN) {
        m_countdown -= dt;
        if (m_countdown <= 0.f) {
            m_countdown = 0.f;
            setPhase(Phase::WAVE_ACTIVE);
        }
        return;
    }

    if (m_phase == Phase::INTERWAVE) {
        m_interWave -= dt;
        if (m_interWave <= 0.f) {
            int next = m_waveIdx + 1;
            if (next >= m_waves.size()) {
                // All waves complete → win
                setPhase(Phase::MISSION_WIN);
                emit missionEnded(true, m_score, m_intercepted, m_missed);
                emit logEvent(QString("MISSION COMPLETE — score %1  intercepts %2/%3")
                              .arg(m_score).arg(m_intercepted).arg(m_intercepted+m_missed));
            } else {
                beginWave(next);
            }
        }
        return;
    }

    if (m_phase == Phase::WAVE_ACTIVE) {
        const WaveDef& wave = m_waves[m_waveIdx];

        // Launch next threat if timer elapsed
        if (m_launchIdx < wave.threats.size()) {
            m_launchTimer -= dt;
            if (m_launchTimer <= 0.f) {
                const WaveThreat& t = wave.threats[m_launchIdx];
                emit launchRequested(t.type, t.azimuth);
                ++m_launchIdx;
                m_launchTimer = wave.intervalSec;
            }
        } else {
            // All launched — wait for targets to land/be intercepted
            if (flyingTargetCount == 0) {
                m_score += 200;  // wave bonus
                m_interWave = 6.f;
                setPhase(Phase::INTERWAVE);
                emit logEvent(QString("WAVE %1 CLEAR — +200 pts").arg(m_waveIdx + 1));
            }
        }
    }
}

void GameState::onGroundImpact(const QVector3D& pos)
{
    m_missed++;
    // Check proximity to each asset
    for (int i = 0; i < m_assets.size(); ++i) {
        auto& a = m_assets[i];
        if (a.destroyed()) continue;
        float dist = (QVector3D(pos.x(), pos.y(), 0) -
                      QVector3D(a.pos.x(), a.pos.y(), 0)).length();
        if (dist <= a.killRadius) {
            a.health = qMax(0.f, a.health - a.damage);
            emit assetDamaged(i, a.health);
            emit logEvent(QString("ASSET HIT: %1  HP=%2%")
                          .arg(a.name).arg(int(a.health)));
            if (a.destroyed())
                emit logEvent(QString("ASSET DESTROYED: %1").arg(a.name));
        }
    }
}

void GameState::onIntercept()
{
    m_intercepted++;
    m_score += 100;
}
