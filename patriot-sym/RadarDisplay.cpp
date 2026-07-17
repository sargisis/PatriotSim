#include "RadarDisplay.h"
#include "Simulation.h"
#include "entities/Missile.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QtMath>
#include <algorithm>

static constexpr float DEG_PER_SEC = 360.f / 8.f;  // one rotation per 8 real seconds

RadarDisplay::RadarDisplay(Simulation* sim, QWidget* parent)
    : QWidget(parent), m_sim(sim)
{
    setMinimumSize(220, 220);
    setMaximumSize(400, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setObjectName("radarDisplay");
    m_lastMs = QDateTime::currentMSecsSinceEpoch();
    startTimer(33);  // ~30 fps, real-time
}

void RadarDisplay::timerEvent(QTimerEvent*)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    float  dt  = (now - m_lastMs) / 1000.f;
    m_lastMs   = now;
    dt = qBound(0.f, dt, 0.1f);

    float prev = m_sweepAngle;
    m_sweepAngle = fmodf(m_sweepAngle + DEG_PER_SEC * dt, 360.f);

    refreshBlips(prev, m_sweepAngle);

    // Age blips
    for (auto& b : m_blips) b.sweepAge += dt;

    // Remove stale blips (older than 1.8 sweep periods)
    m_blips.erase(
        std::remove_if(m_blips.begin(), m_blips.end(),
            [](const Blip& b){ return b.sweepAge > SWEEP_PERIOD * 1.8f; }),
        m_blips.end());

    update();
}

void RadarDisplay::onSimUpdated()
{
    // Data refresh happens in timerEvent — nothing extra needed here
}

float RadarDisplay::bearing(float wx, float wy) const
{
    float a = qRadiansToDegrees(atan2f(wx, wy));
    return fmodf(a + 360.f, 360.f);
}

QPointF RadarDisplay::toScreen(float wx, float wy, QPointF c, float scale) const
{
    return QPointF(c.x() + wx * scale,
                   c.y() - wy * scale);   // y flipped: world N = screen up
}

void RadarDisplay::refreshBlips(float prev, float curr)
{
    const auto& missiles = m_sim->missiles();

    for (const auto& m : missiles) {
        if (m.state != MissileState::Flying) continue;
        float az = bearing(m.pos.x(), m.pos.y());

        // Did sweep cross this azimuth?
        bool swept = false;
        if (prev <= curr) swept = (az >= prev && az < curr);
        else              swept = (az >= prev || az < curr);  // wrapped 360→0

        if (!swept) continue;

        // Update or create blip
        bool found = false;
        for (auto& b : m_blips) {
            if (b.id == m.id) {
                b.wx = m.pos.x(); b.wy = m.pos.y();
                b.sweepAge = 0.f;
                found = true; break;
            }
        }
        if (!found) {
            m_blips.push_back({m.pos.x(), m.pos.y(), 0.f,
                                m.type == MissileType::Target, m.id});
        }
    }
}

void RadarDisplay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int   side   = qMin(width(), height());
    float r      = side * 0.47f;
    QPointF c(width() / 2.f, height() / 2.f);
    QRectF  scopeRect(c.x()-r, c.y()-r, r*2, r*2);
    float   scale = r / DISPLAY_RANGE;

    // ── Background ──────────────────────────────────────────────
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(4, 10, 4));
    p.drawEllipse(scopeRect);

    // ── Range rings ─────────────────────────────────────────────
    p.setBrush(Qt::NoBrush);
    QColor gridCol(18, 55, 18);
    p.setPen(QPen(gridCol, 1));
    for (float rng : {50000.f, 100000.f, 150000.f, 200000.f}) {
        float rr = rng * scale;
        p.drawEllipse(c, rr, rr);
    }

    // ── Azimuth spokes (every 30°) ───────────────────────────────
    p.setPen(QPen(QColor(14, 40, 14), 1));
    for (int az = 0; az < 360; az += 30) {
        float rad = qDegreesToRadians(float(90 - az));
        p.drawLine(c, QPointF(c.x() + r * qCos(rad), c.y() - r * qSin(rad)));
    }

    // ── Sweep glow (filled sector, ~50° trailing arc) ────────────
    {
        float glowDeg = 50.f;
        // Convert sweep bearing to Qt angle: Qt 0=East CCW, bearing 0=North CW
        // bearing → Qt: qtAngle = 90 - bearing
        float qtSweep    = 90.f - m_sweepAngle;
        float qtGlowEnd  = qtSweep;
        float qtGlowStart= qtSweep + glowDeg;  // trailing: CCW (higher Qt) from sweep

        // Draw in 10 slices with increasing alpha toward sweep line
        for (int i = 0; i < 10; ++i) {
            float t0 = float(i)     / 10.f;
            float t1 = float(i + 1) / 10.f;
            float qa  = qtGlowStart + (qtGlowEnd - qtGlowStart) * t0;
            float qb  = qtGlowStart + (qtGlowEnd - qtGlowStart) * t1;
            float span = qb - qa;
            int alpha = int(t0 * 70);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 220, 60, alpha));
            p.drawPie(scopeRect,
                      int(qRound(qa * 16.f)),
                      int(qRound(span * 16.f)));
        }
    }

    // ── Sweep line ───────────────────────────────────────────────
    {
        float rad = qDegreesToRadians(90.f - m_sweepAngle);
        QPointF tip(c.x() + r * qCos(rad), c.y() - r * qSin(rad));
        p.setPen(QPen(QColor(0, 255, 70, 230), 2));
        p.drawLine(c, tip);
        // Bright dot at tip
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 255, 70, 180));
        p.drawEllipse(tip, 3, 3);
    }

    // ── Blips ────────────────────────────────────────────────────
    for (const auto& blip : m_blips) {
        QPointF sp = toScreen(blip.wx, blip.wy, c, scale);
        // Fade over sweep period
        float alpha = qBound(0.f, 1.f - blip.sweepAge / SWEEP_PERIOD, 1.f);

        if (blip.isTarget) {
            int a = int(alpha * 255);
            // Outer glow
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 80, 20, a / 3));
            p.drawEllipse(sp, 9, 9);
            // Core blip
            p.setBrush(QColor(255, 100, 30, a));
            p.drawEllipse(sp, 5, 5);
            // Bright centre
            p.setBrush(QColor(255, 220, 180, a));
            p.drawEllipse(sp, 2, 2);
        } else {
            int a = int(alpha * 200);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 200, 255, a / 3));
            p.drawEllipse(sp, 6, 6);
            p.setBrush(QColor(0, 220, 255, a));
            p.drawEllipse(sp, 3, 3);
        }
    }

    // ── Battery markers ──────────────────────────────────────────
    for (const auto& bty : m_sim->batteries()) {
        QPointF sp = toScreen(bty.pos.x(), bty.pos.y(), c, scale);
        // Triangle pointing up
        QPolygonF tri;
        tri << QPointF(sp.x(),   sp.y()-7)
            << QPointF(sp.x()-5, sp.y()+4)
            << QPointF(sp.x()+5, sp.y()+4);

        // Color by ammo: green=full, yellow=half, red=low, grey=winchester
        QColor col;
        if      (bty.ammo == 0)              col = QColor(80, 80, 80);
        else if (bty.ammo <= bty.maxAmmo/4)  col = QColor(255, 60, 60);
        else if (bty.ammo <= bty.maxAmmo/2)  col = QColor(255, 200, 0);
        else                                 col = QColor(0, 200, 120);

        p.setPen(QPen(col, 1));
        p.setBrush(QColor(col.red(), col.green(), col.blue(), 60));
        p.drawPolygon(tri);

        // Name label
        p.setPen(col);
        QFont f; f.setPixelSize(9); f.setBold(true); p.setFont(f);
        p.drawText(QPointF(sp.x()+7, sp.y()+4), bty.name);
    }

    // ── Range labels ─────────────────────────────────────────────
    {
        QFont f; f.setPixelSize(9); p.setFont(f);
        p.setPen(QColor(30, 90, 30));
        for (float rng : {50000.f, 100000.f, 150000.f}) {
            float rr = rng * scale;
            p.drawText(QPointF(c.x() + 2, c.y() - rr + 10),
                       QString("%1").arg(int(rng/1000)));
        }
    }

    // ── Cardinal directions ──────────────────────────────────────
    {
        QFont f; f.setPixelSize(10); f.setBold(true); p.setFont(f);
        p.setPen(QColor(60, 140, 60));
        float margin = r + 12.f;
        p.drawText(QPointF(c.x()-4,   c.y()-margin),     "N");
        p.drawText(QPointF(c.x()-4,   c.y()+margin+6),   "S");
        p.drawText(QPointF(c.x()+margin, c.y()+4),       "E");
        p.drawText(QPointF(c.x()-margin-10, c.y()+4),    "W");
    }

    // ── Scope border ─────────────────────────────────────────────
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 100, 0), 2));
    p.drawEllipse(scopeRect);

    // ── Title ────────────────────────────────────────────────────
    {
        QFont f; f.setPixelSize(9); p.setFont(f);
        p.setPen(QColor(0, 160, 60));
        p.drawText(QRectF(0, 4, width(), 16), Qt::AlignHCenter,
                   QString("AN/MPQ-65  RANGE %1 km").arg(int(DISPLAY_RANGE/1000)));
    }
}
