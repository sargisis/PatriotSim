#pragma once
#include <QMap>
#include <QString>
#include <QVector>

// ── State passed from Simulation to AI per engagement decision ──
struct AIEngagementState {
    // Target properties (6)
    float altNorm;        // pos.z / 150000
    float speedNorm;      // vel.length / 4000
    float threatNorm;     // (int)ThreatType / 4
    float maneuvering;    // 0 or 1
    float assetThreat;    // 1 - min_dist_to_asset/20000, clamped [0,1]
    float headingInward;  // dot(vel_hat, -pos_hat), clamped [0,1]
    // Per-battery (4 × 3 = 12)
    float eligible[4];    // 1 if can engage (ammo>0, not reloading, alt in band)
    float tgoNorm[4];     // T_go / 60, 0 if not eligible
    float ammoNorm[4];    // ammo / maxAmmo
};

static constexpr int AI_N_FEAT = 18;  // 6 + 4 + 4 + 4
static constexpr int AI_N_ACT  = 5;   // batteries 0-3 + hold(4)

// ── Q-learning engagement agent ─────────────────────────────────
class EngagementAI
{
public:
    EngagementAI();

    // Decision: returns battery index 0-3, or -1 for hold
    // targetId used to store experience for later reward
    int decide(int targetId, const AIEngagementState& s);

    // Outcome signals — call from Simulation
    void onIntercept(int targetId);
    void onGroundImpact(int targetId, bool hitAsset);
    void onInterceptorWasted(int targetId);  // interceptor missed, target already gone

    // Episode bookkeeping (call per mission)
    void beginEpisode();
    void endEpisode(bool win);

    // Getters for HUD
    float epsilon()     const { return m_epsilon; }
    int   episodes()    const { return m_episodes; }
    float avgReward()   const;
    float interceptRate() const;
    int   totalDecisions() const { return m_totalDecisions; }

    bool save(const QString& path) const;
    bool load(const QString& path);

    // Enable/disable AI (falls back to greedy T_go)
    bool enabled()        const { return m_enabled; }
    void setEnabled(bool e)     { m_enabled = e; }

private:
    struct PendingExp {
        float feat[AI_N_FEAT];
        int   action;
    };

    float m_w[AI_N_ACT][AI_N_FEAT];

    // Hyperparameters
    float m_epsilon      = 0.30f;
    float m_epsilonMin   = 0.05f;
    float m_epsilonDecay = 0.992f;
    float m_alpha        = 0.04f;   // learning rate
    float m_gamma        = 0.85f;   // discount (single-step so mostly 0)

    // State
    bool  m_enabled      = true;
    int   m_episodes     = 0;
    int   m_totalDecisions = 0;

    QMap<int, PendingExp> m_pending;  // targetId → last decision

    // Rolling stats (ring buffer of last 50 rewards)
    static constexpr int STAT_BUF = 50;
    float m_rewardBuf[STAT_BUF] = {};
    int   m_rewardHead = 0;
    int   m_rewardCount= 0;

    int   m_episodeHits   = 0;
    int   m_episodeMisses = 0;
    int   m_interceptsTotal = 0;
    int   m_missesTotal     = 0;

    static void stateToFeat(const AIEngagementState& s, float* out);
    float qValue(const float* feat, int action) const;
    int   greedyAction(const float* feat, const AIEngagementState& s) const;
    void  tdUpdate(const float* feat, int action, float reward);
    void  pushReward(float r);
    void  decayEpsilon();
};
