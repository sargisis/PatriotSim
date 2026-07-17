#pragma once
#include "entities/Missile.h"
#include "entities/ThreatType.h"
#include "entities/ProtectedAsset.h"
#include "GameState.h"
#include "EngagementAI.h"
#include "physics/Ballistic.h"
#include <QObject>
#include <QTimer>
#include <QVector3D>
#include <QVector>
#include <QSet>
#include <QMap>

class Simulation : public QObject
{
    Q_OBJECT
public:
    struct LaunchParams {
        float speed     = 3000.f;
        float elevation = 45.f;
        float azimuth   = 0.f;
        float windSpeed = 0.f;
        float windDir   = 270.f;
        float latitude  = 40.18f;
        float mass      = 2000.f;
        float diameter  = 0.88f;
        float cd0         = 0.3f;
        bool  maneuvering = false;
    };

    struct LaunchBattery {
        QVector3D    pos;
        QString      name;
        WeaponSystem wsys       = WeaponSystem::PAC_3;
        int          ammo       = 12;
        int          maxAmmo    = 12;
        float        intSpeed   = 1700.f;    // interceptor speed m/s (PAC-3 MSE: 1700, THAAD: 2800, Tamir: 750)
        float        killRad    = 20.f;      // kill radius m
        float        minAlt     = 500.f;     // min intercept altitude m
        float        maxAlt     = 24000.f;   // max intercept altitude m
        float        reloadTimer= 0.f;       // >0 means reloading
        float        pkNormal   = 0.80f;     // Pk vs non-maneuvering target
        float        pkManeuver = 0.55f;     // Pk vs maneuvering target
    };

    explicit Simulation(QObject* parent = nullptr);

    void setParams(const LaunchParams& p) { m_params = p; }
    const LaunchParams& params() const    { return m_params; }

    void launchTarget(QVector3D impactHint = {});
    void launchThreatType(ThreatType type, float azimuth);   // game mode
    void launchInterceptorAt(int targetId, int batteryIdx);
    void reset();
    void togglePause();
    bool isPaused() const { return m_paused; }

    void setAutoIntercept(bool on) { m_autoIntercept = on; }
    bool autoIntercept() const     { return m_autoIntercept; }

    void setSpeedMult(int m)  { m_speedMult = qBound(1, m, 4); }
    int  speedMult() const    { return m_speedMult; }

    bool hasRadarContact()       const;
    bool hasActiveInterceptors() const;

    GameState*                    gameState()  const { return m_game; }
    EngagementAI*                 ai()         const { return m_ai; }
    const QVector<Missile>&       missiles()   const { return m_missiles; }
    const QVector<Explosion>&     explosions() const { return m_explosions; }
    const QVector<LaunchBattery>& batteries()  const { return m_batteries; }
    float elapsedTime() const { return m_time; }

signals:
    void updated();
    void eventLogged(const QString& msg);
    void ammoBatteryUpdated(int batteryIdx, int ammo, int maxAmmo);
    void mirvSplit(int parentId, QVector3D pos, QVector3D vel);  // for renderer flash

private slots:
    void tick();
    void onGameLaunchRequested(ThreatType type, float azimuth);

private:
    void stepPhysics(float dt);
    void stepMissile(Missile& m, float dt);
    void updateManeuver(Missile& m, float dt);
    void detectAndAutoLaunch();
    void checkInterceptions();
    AIEngagementState buildAIState(const Missile& tgt, int flyingCount) const;
    void checkMirvSplit();      // detect apex crossing and spawn sub-warheads
    void updateReload(float dt);
    void addExplosion(QVector3D pos);

    Missile makeMissileFromThreat(ThreatType type, float azimuth);

    QTimer              m_timer;
    GameState*          m_game  = nullptr;
    EngagementAI*       m_ai   = nullptr;
    QVector<Missile>        m_missiles;
    QVector<Explosion>      m_explosions;
    QVector<LaunchBattery>  m_batteries;
    QVector<Missile>        m_spawnQueue;  // MIRV sub-warheads to add next frame

    LaunchParams  m_params;
    float         m_time         = 0.f;
    bool          m_paused       = false;
    bool          m_autoIntercept= true;
    int           m_speedMult    = 1;
    int           m_nextId       = 0;
    QSet<int>     m_radarAcquired;
    QMap<int,int> m_totalFired;

    static constexpr float DT           = 1.f / 60.f;
    static constexpr float RADAR_RANGE  = 150000.f;
    static constexpr float REACTION_TIME= 2.f;
    static constexpr int   MAX_INT_FLYING  = 2;
    static constexpr int   MAX_INT_TOTAL   = 4;
};
