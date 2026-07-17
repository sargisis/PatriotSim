#pragma once
#include <QWidget>
#include <QVector>

class Simulation;

class RadarDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit RadarDisplay(Simulation* sim, QWidget* parent = nullptr);

    static constexpr float DISPLAY_RANGE = 200000.f;
    static constexpr float SWEEP_PERIOD  = 8.f;

public slots:
    void onSimUpdated();

protected:
    void paintEvent(QPaintEvent*) override;
    void timerEvent(QTimerEvent*) override;

private:
    struct Blip {
        float  wx, wy;
        float  sweepAge;
        bool   isTarget;
        int    id;
    };

    void    refreshBlips(float prevAngle, float newAngle);
    QPointF toScreen(float wx, float wy, QPointF c, float scale) const;
    float   bearing(float wx, float wy) const;

    Simulation*   m_sim;
    float         m_sweepAngle = 0.f;
    qint64        m_lastMs     = 0;
    QVector<Blip> m_blips;
};
