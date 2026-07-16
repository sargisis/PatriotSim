#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include "entities/ThreatType.h"
#include "entities/ProtectedAsset.h"

// One threat in a wave
struct WaveThreat {
    ThreatType type;
    float      azimuth;   // degrees from North
};

// Definition of a single wave
struct WaveDef {
    QVector<WaveThreat> threats;
    float               intervalSec = 4.f;   // delay between individual launches
    float               prePauseSec = 8.f;   // countdown before wave starts
};

// Pre-built scenario
struct ScenarioDef {
    QString         name;
    QString         briefing;
    QVector<WaveDef> waves;
    QVector<ProtectedAsset> assets;
};

class GameState : public QObject
{
    Q_OBJECT
public:
    enum class Phase {
        IDLE,
        COUNTDOWN,      // pre-wave pause, m_countdown ticking down
        WAVE_ACTIVE,    // threats being launched / in flight
        INTERWAVE,      // brief pause between waves
        MISSION_WIN,
        MISSION_FAIL
    };

    explicit GameState(QObject* parent = nullptr);

    // Call once to load a scenario
    void startScenario(int idx);
    void reset();

    // Called every simulation tick (dt = sim delta, not real)
    void tick(float dt, int flyingTargetCount);

    // Called by Simulation when target hits ground (not intercepted)
    void onGroundImpact(const QVector3D& pos);

    // Called by Simulation when an intercept happens (for score)
    void onIntercept();

    // Scenario list
    static int scenarioCount() { return 3; }
    static QString scenarioName(int idx);
    static QString scenarioBriefing(int idx);

    // Getters
    Phase   phase()       const { return m_phase; }
    int     currentWave() const { return m_waveIdx + 1; }
    int     totalWaves()  const { return m_waves.size(); }
    float   countdown()   const { return m_countdown; }
    int     score()       const { return m_score; }
    int     intercepted() const { return m_intercepted; }
    int     missed()      const { return m_missed; }
    bool    active()      const { return m_phase != Phase::IDLE &&
                                         m_phase != Phase::MISSION_WIN &&
                                         m_phase != Phase::MISSION_FAIL; }

    const QVector<ProtectedAsset>& assets() const { return m_assets; }
          QVector<ProtectedAsset>& assets()       { return m_assets; }

signals:
    // Simulation should connect to this and launch the given threat
    void launchRequested(ThreatType type, float azimuth);

    void waveStarted(int wave, int total);
    void missionEnded(bool win, int score, int intercepted, int missed);
    void assetDamaged(int assetIdx, float newHealth);
    void phaseChanged(Phase newPhase);
    void logEvent(const QString& msg);

private:
    void setPhase(Phase p);
    void beginWave(int idx);
    bool allAssetsDestroyed() const;
    void buildScenario(int idx);

    Phase   m_phase    = Phase::IDLE;
    int     m_waveIdx  = -1;
    int     m_score    = 0;
    int     m_intercepted = 0;
    int     m_missed      = 0;

    float   m_countdown    = 0.f;
    float   m_launchTimer  = 0.f;
    int     m_launchIdx    = 0;      // next threat to launch in current wave
    float   m_interWave    = 0.f;   // between-wave pause timer

    QVector<WaveDef>        m_waves;
    QVector<ProtectedAsset> m_assets;
};
