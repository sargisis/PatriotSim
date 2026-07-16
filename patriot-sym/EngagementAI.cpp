#include "EngagementAI.h"
#include <QtMath>
#include <QFile>
#include <QDataStream>
#include <cstring>
#include <cstdlib>

EngagementAI::EngagementAI()
{
    // Initialize weights to small random values
    for (int a = 0; a < AI_N_ACT; ++a)
        for (int f = 0; f < AI_N_FEAT; ++f)
            m_w[a][f] = (float(rand()) / RAND_MAX - 0.5f) * 0.1f;
}

// ── Feature extraction ─────────────────────────────────────────
void EngagementAI::stateToFeat(const AIEngagementState& s, float* out)
{
    out[0]  = qBound(0.f, s.altNorm,      1.f);
    out[1]  = qBound(0.f, s.speedNorm,    1.f);
    out[2]  = qBound(0.f, s.threatNorm,   1.f);
    out[3]  = s.maneuvering;
    out[4]  = qBound(0.f, s.assetThreat,  1.f);
    out[5]  = qBound(0.f, s.headingInward,1.f);
    // Battery eligibility
    out[6]  = s.eligible[0];
    out[7]  = s.eligible[1];
    out[8]  = s.eligible[2];
    out[9]  = s.eligible[3];
    // T_go normalized
    out[10] = qBound(0.f, s.tgoNorm[0],   1.f);
    out[11] = qBound(0.f, s.tgoNorm[1],   1.f);
    out[12] = qBound(0.f, s.tgoNorm[2],   1.f);
    out[13] = qBound(0.f, s.tgoNorm[3],   1.f);
    // Ammo fraction
    out[14] = s.ammoNorm[0];
    out[15] = s.ammoNorm[1];
    out[16] = s.ammoNorm[2];
    out[17] = s.ammoNorm[3];
}

// ── Q-value ────────────────────────────────────────────────────
float EngagementAI::qValue(const float* feat, int action) const
{
    float q = 0.f;
    for (int f = 0; f < AI_N_FEAT; ++f)
        q += m_w[action][f] * feat[f];
    return q;
}

// ── Greedy action respecting eligibility constraints ───────────
int EngagementAI::greedyAction(const float* feat, const AIEngagementState& s) const
{
    int   bestAct = 4;  // hold by default
    float bestQ   = qValue(feat, 4);

    for (int a = 0; a < 4; ++a) {
        if (s.eligible[a] < 0.5f) continue;   // can't fire
        float q = qValue(feat, a);
        if (q > bestQ) { bestQ = q; bestAct = a; }
    }
    return bestAct;
}

// ── TD(0) weight update ────────────────────────────────────────
void EngagementAI::tdUpdate(const float* feat, int action, float reward)
{
    float q   = qValue(feat, action);
    float err = reward - q;   // terminal step: no next-state value
    for (int f = 0; f < AI_N_FEAT; ++f)
        m_w[action][f] += m_alpha * err * feat[f];
}

// ── Rolling stats ─────────────────────────────────────────────
void EngagementAI::pushReward(float r)
{
    m_rewardBuf[m_rewardHead % STAT_BUF] = r;
    m_rewardHead++;
    if (m_rewardCount < STAT_BUF) ++m_rewardCount;
}

void EngagementAI::decayEpsilon()
{
    m_epsilon = qMax(m_epsilonMin, m_epsilon * m_epsilonDecay);
}

// ── Main decision ──────────────────────────────────────────────
int EngagementAI::decide(int targetId, const AIEngagementState& s)
{
    float feat[AI_N_FEAT];
    stateToFeat(s, feat);

    int action;
    if (!m_enabled) {
        // Fallback: pick eligible battery with best T_go
        int best = -1; float bestTgo = 1e9f;
        for (int i = 0; i < 4; ++i)
            if (s.eligible[i] > 0.5f && s.tgoNorm[i] < bestTgo)
            { bestTgo = s.tgoNorm[i]; best = i; }
        action = (best >= 0) ? best : 4;
    } else {
        // ε-greedy
        float rnd = float(rand()) / RAND_MAX;
        if (rnd < m_epsilon) {
            // Random among eligible batteries + hold
            QVector<int> valid;
            valid.push_back(4);  // hold always valid
            for (int i = 0; i < 4; ++i)
                if (s.eligible[i] > 0.5f) valid.push_back(i);
            action = valid[rand() % valid.size()];
        } else {
            action = greedyAction(feat, s);
        }
    }

    // Store experience for reward assignment
    PendingExp exp;
    std::memcpy(exp.feat, feat, sizeof(feat));
    exp.action = action;
    m_pending[targetId] = exp;

    ++m_totalDecisions;
    return (action < 4) ? action : -1;
}

// ── Reward signals ─────────────────────────────────────────────
void EngagementAI::onIntercept(int targetId)
{
    auto it = m_pending.find(targetId);
    if (it == m_pending.end()) return;

    const float reward = 10.f;
    if (m_enabled) tdUpdate(it->feat, it->action, reward);
    pushReward(reward);
    m_pending.erase(it);
    decayEpsilon();
    ++m_interceptsTotal;
    ++m_episodeHits;
}

void EngagementAI::onGroundImpact(int targetId, bool hitAsset)
{
    auto it = m_pending.find(targetId);
    if (it == m_pending.end()) {
        pushReward(hitAsset ? -25.f : -15.f);
        ++m_missesTotal; ++m_episodeMisses;
        return;
    }

    const float reward = hitAsset ? -25.f : -15.f;
    if (m_enabled) tdUpdate(it->feat, it->action, reward);
    pushReward(reward);
    m_pending.erase(it);
    decayEpsilon();
    ++m_missesTotal;
    ++m_episodeMisses;
}

void EngagementAI::onInterceptorWasted(int targetId)
{
    // Small negative: fired but target was already dead or interceptor missed
    auto it = m_pending.find(targetId);
    if (it == m_pending.end()) return;
    if (m_enabled) tdUpdate(it->feat, it->action, -1.f);
    pushReward(-1.f);
}

// ── Episode bookkeeping ────────────────────────────────────────
void EngagementAI::beginEpisode()
{
    m_episodeHits   = 0;
    m_episodeMisses = 0;
}

void EngagementAI::endEpisode(bool win)
{
    ++m_episodes;
    if (win) {
        // Bonus reward for mission success — update all pending
        for (auto& exp : m_pending)
            if (m_enabled) tdUpdate(exp.feat, exp.action, 5.f);
    }
    m_pending.clear();
}

// ── Stats ──────────────────────────────────────────────────────
float EngagementAI::avgReward() const
{
    if (m_rewardCount == 0) return 0.f;
    float sum = 0.f;
    int n = qMin(m_rewardCount, STAT_BUF);
    for (int i = 0; i < n; ++i) sum += m_rewardBuf[i];
    return sum / n;
}

float EngagementAI::interceptRate() const
{
    int total = m_interceptsTotal + m_missesTotal;
    return total > 0 ? float(m_interceptsTotal) / total : 0.f;
}

// ── Persistence ────────────────────────────────────────────────
bool EngagementAI::save(const QString& path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);

    ds << quint32(0xA1B2C3D4);   // magic
    ds << quint32(1);             // version
    ds << m_epsilon << m_episodes << m_interceptsTotal << m_missesTotal;

    for (int a = 0; a < AI_N_ACT; ++a)
        for (int ff = 0; ff < AI_N_FEAT; ++ff)
            ds << m_w[a][ff];

    return ds.status() == QDataStream::Ok;
}

bool EngagementAI::load(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDataStream ds(&f);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint32 magic, ver;
    ds >> magic >> ver;
    if (magic != 0xA1B2C3D4 || ver != 1) return false;

    ds >> m_epsilon >> m_episodes >> m_interceptsTotal >> m_missesTotal;

    for (int a = 0; a < AI_N_ACT; ++a)
        for (int ff = 0; ff < AI_N_FEAT; ++ff)
            ds >> m_w[a][ff];

    return ds.status() == QDataStream::Ok;
}
