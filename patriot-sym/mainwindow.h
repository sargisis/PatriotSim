#pragma once
#include "Simulation.h"
#include "GameState.h"
#include "RadarDisplay.h"
#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QDockWidget>
#include <QButtonGroup>
#include <QFormLayout>
#include <QComboBox>
#include <QProgressBar>
#include <QStyle>
#include <QTimer>

class SimRenderer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent*) override;

private slots:
    void onLaunchTarget();
    void onReset();
    void onGroundClicked(QVector3D wp);
    void onSimUpdated();
    void onStartMission();
    void onMissionEnded(bool win, int score, int intercepted, int missed);
    void onWaveStarted(int wave, int total);
    void onAssetDamaged(int idx, float health);

private:
    // helpers
    void buildUI();
    QWidget* buildRightDockContent();
    QWidget* buildMissionDockContent();
    QWidget* buildStatusBarWidget();
    void addSliderRow(QFormLayout* f, const QString& lbl,
                      QSlider*& sl, QDoubleSpinBox*& sp,
                      double lo, double hi, double val,
                      int scale=1, const QString& suf={});
    Simulation::LaunchParams collectParams() const;
    void logEvent(const QString& msg);
    void setIndicator(QLabel* lbl, bool active);

    // Core objects
    Simulation*  m_sim      = nullptr;
    SimRenderer* m_renderer = nullptr;

    // Status bar labels
    QLabel* m_lblRadar    = nullptr;
    QLabel* m_lblTracking = nullptr;
    QLabel* m_lblWeapons  = nullptr;
    QLabel* m_lblSimTime  = nullptr;

    // Parameter sliders / spinboxes
    QSlider* m_slSpeed=nullptr;     QDoubleSpinBox* m_sbSpeed=nullptr;
    QSlider* m_slElev=nullptr;      QDoubleSpinBox* m_sbElev=nullptr;
    QSlider* m_slAz=nullptr;        QDoubleSpinBox* m_sbAz=nullptr;
    QSlider* m_slMass=nullptr;      QDoubleSpinBox* m_sbMass=nullptr;
    QSlider* m_slDiam=nullptr;      QDoubleSpinBox* m_sbDiam=nullptr;
    QSlider* m_slWind=nullptr;      QDoubleSpinBox* m_sbWind=nullptr;
    QSlider* m_slWindDir=nullptr;   QDoubleSpinBox* m_sbWindDir=nullptr;
    QSlider* m_slLat=nullptr;       QDoubleSpinBox* m_sbLat=nullptr;
    QCheckBox* m_cbCoriolis    = nullptr;
    QCheckBox* m_cbClickMode   = nullptr;
    QCheckBox* m_cbManeuver    = nullptr;

    // Buttons
    QPushButton* m_btnAutoIntercept = nullptr;
    QPushButton* m_btnPause         = nullptr;
    QButtonGroup* m_speedGroup      = nullptr;

    // Radar display
    RadarDisplay* m_radar = nullptr;

    // Mission panel widgets
    QComboBox*    m_cbScenario    = nullptr;
    QPushButton*  m_btnMission    = nullptr;
    QLabel*       m_lblWaveStatus = nullptr;
    QLabel*       m_lblScore      = nullptr;
    QLabel*       m_lblBriefing   = nullptr;
    QVector<QProgressBar*> m_assetBars;

    // Log
    QTextEdit* m_log = nullptr;

    // Click-to-aim state
    QVector3D m_pendingImpact;
};
