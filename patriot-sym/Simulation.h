#pragma once
#include "entities/Missile.h"
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
        float speed     = 1200.f; // m/s
        float elevation = 45.f;   // degrees above horizon
        float azimuth   = 0.f;    // degrees from North, clockwise
        float windSpeed = 0.f;
        float windDir   = 270.f;
        float latitude  = 40.18f;
        float mass      = 800.f;
        float diameter  = 0.88f;
        float cd0         = 0.3f;
        bool  maneuvering = false;  // terminal-phase evasion maneuver
    };

    struct LaunchBattery {
        QVector3D pos;
        QString   name;
        int       ammo    = 8;
        int       maxAmmo = 8;
    };

    explicit Simulation(QObject* parent = nullptr);

    void setParams(const LaunchParams& p) { m_params = p; }
    const LaunchParams& params() const    { return m_params; }

    void launchTarget(QVector3D impactHint = {});
    void launchInterceptorAt(int targetId, int batteryIdx);
    void reset();
    void togglePause();
    bool isPaused() const { return m_paused; }

    void setAutoIntercept(bool on) { m_autoIntercept = on; }
    bool autoIntercept() const     { return m_autoIntercept; }

    void setSpeedMult(int m)  { m_speedMult = qBound(1, m, 4); }
    int  speedMult() const    { return m_speedMult; }

    bool hasRadarContact()        const;
    bool hasActiveInterceptors()  const;

    const QVector<Missile>&       missiles()   const { return m_missiles; }
    const QVector<Explosion>&     explosions() const { return m_explosions; }
    const QVector<LaunchBattery>& batteries()  const { return m_batteries; }
    float elapsedTime() const { return m_time; }

signals:
    void updated();
    void eventLogged(const QString& msg);
    void ammoBatteryUpdated(int batteryIdx, int ammo, int maxAmmo);

private slots:
    void tick();

private:
    void stepPhysics(float dt);
    void stepMissile(Missile& m, float dt);
    void updateManeuver(Missile& m, float dt);
    void detectAndAutoLaunch();
    void checkInterceptions();
    void addExplosion(QVector3D pos);

    QTimer             m_timer;
    QVector<Missile>       m_missiles;
    QVector<Explosion>     m_explosions;
    QVector<LaunchBattery> m_batteries;
    LaunchParams  m_params;
    float         m_time         = 0.f;
    bool          m_paused       = false;
    bool          m_autoIntercept= true;
    int           m_speedMult    = 1;
    int           m_nextId       = 0;
    QSet<int>        m_radarAcquired;
    QMap<int,int>    m_totalFired;   // targetId → total interceptors fired (all-time)

    static constexpr float DT            = 1.f / 60.f;
    static constexpr float RADAR_RANGE   = 150000.f;  // 150 km — shared networked radar
    static constexpr float KILL_RADIUS   = 75.f;      // m
    static constexpr float REACTION_TIME = 2.f;       // s
    static constexpr float INT_SPEED     = 2500.f;    // m/s
    static constexpr int   MAX_INT_FLYING   = 2;      // max simultaneously flying at one target
    static constexpr int   MAX_INT_TOTAL    = 4;      // max ever fired at one target
};
