#include "mainwindow.h"
#include "SimRenderer.h"
#include <QDockWidget>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QFrame>
#include <QSettings>
#include <QCloseEvent>
#include <QScrollBar>
#include <QTime>
#include <QtMath>

// ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("PatriotSim  ▸  PAC-3 Ballistic Intercept Simulator");
    resize(1400, 880);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

    buildUI();

    // Restore previous layout
    QSettings cfg("PatriotSim","PatriotSim");
    if(cfg.contains("geometry")) restoreGeometry(cfg.value("geometry").toByteArray());
    if(cfg.contains("state"))    restoreState(cfg.value("state").toByteArray());
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent* e)
{
    QSettings cfg("PatriotSim","PatriotSim");
    cfg.setValue("geometry", saveGeometry());
    cfg.setValue("state",    saveState());
    e->accept();
}

// ─── Status bar widget (embedded in top toolbar) ──────────────

QWidget* MainWindow::buildStatusBarWidget()
{
    auto* w = new QWidget;
    w->setObjectName("statusBarWidget");
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(6,0,6,0);
    lay->setSpacing(0);

    auto makeIndicator=[&](const QString& name, QLabel*& dot)->QWidget*{
        auto* cell=new QWidget;
        auto* hl=new QHBoxLayout(cell);
        hl->setContentsMargins(8,0,8,0);
        hl->setSpacing(5);
        dot=new QLabel("●");
        dot->setProperty("indicator","inactive");
        QLabel* lbl=new QLabel(name);
        lbl->setStyleSheet("color:#3A4A5A;font-size:11px;font-weight:bold;");
        hl->addWidget(dot);
        hl->addWidget(lbl);
        return cell;
    };

    lay->addWidget(makeIndicator("RADAR",    m_lblRadar));
    lay->addWidget(makeIndicator("TRACKING", m_lblTracking));
    lay->addWidget(makeIndicator("WEAPONS",  m_lblWeapons));

    // Centre separator + time
    auto* sep=new QFrame; sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color:#1E2A38;margin:6px 2px;");
    lay->addWidget(sep);
    lay->addStretch(1);

    m_lblSimTime=new QLabel("SIM TIME  00:00.0");
    m_lblSimTime->setObjectName("lblSimTime");
    lay->addWidget(m_lblSimTime, 0, Qt::AlignCenter);

    lay->addStretch(1);
    auto* sep2=new QFrame; sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color:#1E2A38;margin:6px 2px;");
    lay->addWidget(sep2);

    // Speed buttons
    auto* speedBox=new QWidget;
    auto* sbl=new QHBoxLayout(speedBox);
    sbl->setContentsMargins(4,0,0,0);
    sbl->setSpacing(2);
    m_speedGroup=new QButtonGroup(this);
    m_speedGroup->setExclusive(true);

    for(auto [lbl,mult]:std::initializer_list<std::pair<const char*,int>>{{"×1",1},{"×2",2},{"×4",4}}){
        auto* btn=new QPushButton(lbl);
        btn->setObjectName("btnSpeed");
        btn->setCheckable(true);
        if(mult==1) btn->setChecked(true);
        m_speedGroup->addButton(btn,mult);
        sbl->addWidget(btn);
    }
    lay->addWidget(speedBox);

    connect(m_speedGroup, &QButtonGroup::idClicked, this, [this](int id){
        m_sim->setSpeedMult(id);
    });

    return w;
}

// ─── Slider+spinbox helper ────────────────────────────────────

void MainWindow::addSliderRow(QFormLayout* form, const QString& lbl,
                               QSlider*& sl, QDoubleSpinBox*& sp,
                               double lo, double hi, double val,
                               int scale, const QString& suf)
{
    sp=new QDoubleSpinBox;
    sp->setRange(lo,hi);
    sp->setValue(val);
    sp->setDecimals(scale>1?1:0);
    sp->setSingleStep(scale>1?0.5:1.0);
    if(!suf.isEmpty()) sp->setSuffix(suf);
    sp->setFixedWidth(86);

    sl=new QSlider(Qt::Horizontal);
    sl->setRange(int(lo*scale),int(hi*scale));
    sl->setValue(int(val*scale));

    // slider → spin
    connect(sl,&QSlider::valueChanged,[sp,scale](int v){
        QSignalBlocker b(sp);
        sp->setValue(double(v)/scale);
    });
    // spin → slider
    connect(sp,QOverload<double>::of(&QDoubleSpinBox::valueChanged),[sl,scale](double v){
        QSignalBlocker b(sl);
        sl->setValue(int(v*scale));
    });

    auto* row=new QWidget;
    auto* hl=new QHBoxLayout(row);
    hl->setContentsMargins(0,0,0,0);
    hl->setSpacing(6);
    hl->addWidget(sl,1);
    hl->addWidget(sp);

    form->addRow(lbl, row);
}

// ─── Right dock content ───────────────────────────────────────

QWidget* MainWindow::buildRightDockContent()
{
    auto* w=new QWidget;
    auto* vl=new QVBoxLayout(w);
    vl->setContentsMargins(8,8,8,8);
    vl->setSpacing(10);

    // ── TARGET LAUNCH group ──────────────────────────────────
    auto* grpLaunch=new QGroupBox("TARGET LAUNCH");
    auto* fl=new QFormLayout(grpLaunch);
    fl->setSpacing(8);
    fl->setContentsMargins(6,4,6,6);

    addSliderRow(fl,"SPEED",  m_slSpeed, m_sbSpeed, 500,3000,1200,1," m/s");
    addSliderRow(fl,"ELEV",   m_slElev,  m_sbElev,  20,85,45,   2,"°");
    addSliderRow(fl,"AZIMUTH",m_slAz,   m_sbAz,    0,360,0,    1,"°");
    addSliderRow(fl,"MASS",   m_slMass,  m_sbMass,  200,2000,800,1," kg");
    addSliderRow(fl,"DIAM",   m_slDiam,  m_sbDiam,  0.3,1.5,0.88,10," m");

    vl->addWidget(grpLaunch);

    // ── ENVIRONMENT group ────────────────────────────────────
    auto* grpEnv=new QGroupBox("ENVIRONMENT");
    auto* fe=new QFormLayout(grpEnv);
    fe->setSpacing(8);
    fe->setContentsMargins(6,4,6,6);

    addSliderRow(fe,"WIND",    m_slWind,    m_sbWind,    0,30,0,   2," m/s");
    addSliderRow(fe,"W.DIR",   m_slWindDir, m_sbWindDir, 0,360,270,1,"°");
    addSliderRow(fe,"LATITUDE",m_slLat,     m_sbLat,     -90,90,40.18,10,"°");

    m_cbCoriolis=new QCheckBox("Coriolis Effect");
    m_cbCoriolis->setChecked(true);
    fe->addRow("",m_cbCoriolis);

    vl->addWidget(grpEnv);

    // ── Click-to-aim ─────────────────────────────────────────
    m_cbClickMode=new QCheckBox("Click-to-aim  (LMB on 3D view)");
    vl->addWidget(m_cbClickMode);

    m_cbManeuver=new QCheckBox("Maneuvering  (TBM evasion 3.5g)");
    vl->addWidget(m_cbManeuver);

    // ── Divider ──────────────────────────────────────────────
    auto* line=new QFrame; line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color:#1E2A38;");
    vl->addWidget(line);

    // ── Action buttons ───────────────────────────────────────
    auto* btnLaunch=new QPushButton("◈  LAUNCH TARGET");
    btnLaunch->setObjectName("btnLaunch");
    vl->addWidget(btnLaunch);

    m_btnAutoIntercept=new QPushButton("AUTO INTERCEPT: ON");
    m_btnAutoIntercept->setObjectName("btnAutoIntercept");
    m_btnAutoIntercept->setCheckable(true);
    m_btnAutoIntercept->setChecked(true);
    vl->addWidget(m_btnAutoIntercept);

    auto* btnRow=new QWidget;
    auto* brl=new QHBoxLayout(btnRow);
    brl->setContentsMargins(0,0,0,0);
    brl->setSpacing(4);

    m_btnPause=new QPushButton("⏸ PAUSE");
    m_btnPause->setObjectName("btnPause");
    auto* btnCam=new QPushButton("⊹ CAM");
    btnCam->setObjectName("btnCam");
    auto* btnReset=new QPushButton("↺ RESET");
    btnReset->setObjectName("btnReset");

    brl->addWidget(m_btnPause,1);
    brl->addWidget(btnCam,1);
    brl->addWidget(btnReset,1);
    vl->addWidget(btnRow);

    vl->addStretch();

    // Wire buttons
    connect(btnLaunch,  &QPushButton::clicked, this, &MainWindow::onLaunchTarget);
    connect(btnReset,   &QPushButton::clicked, this, &MainWindow::onReset);
    connect(m_btnPause, &QPushButton::clicked, this, [this]{
        m_sim->togglePause();
        m_btnPause->setText(m_sim->isPaused() ? "▶ RESUME" : "⏸ PAUSE");
    });
    connect(btnCam, &QPushButton::clicked, m_renderer, &SimRenderer::resetCamera);
    connect(m_btnAutoIntercept, &QPushButton::toggled, this, [this](bool on){
        m_sim->setAutoIntercept(on);
        m_btnAutoIntercept->setText(on ? "AUTO INTERCEPT: ON" : "AUTO INTERCEPT: OFF");
    });

    return w;
}

// ─── Main build ───────────────────────────────────────────────

void MainWindow::buildUI()
{
    m_sim = new Simulation(this);
    connect(m_sim, &Simulation::eventLogged, this, &MainWindow::logEvent);
    connect(m_sim, &Simulation::updated,     this, &MainWindow::onSimUpdated);

    // Central widget = renderer
    m_renderer = new SimRenderer(m_sim, this);
    connect(m_renderer, &SimRenderer::groundClicked, this, &MainWindow::onGroundClicked);
    setCentralWidget(m_renderer);

    // Top toolbar / status bar
    auto* toolbar = new QToolBar("Status");
    toolbar->setObjectName("statusToolbar");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->addWidget(buildStatusBarWidget());
    addToolBar(Qt::TopToolBarArea, toolbar);

    // Right dock — parameters
    auto* rightDock = new QDockWidget("TARGET CONTROL", this);
    rightDock->setObjectName("rightDock");
    rightDock->setWidget(buildRightDockContent());
    rightDock->setMinimumWidth(300);
    rightDock->setMaximumWidth(360);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    // Left dock — radar display
    m_radar = new RadarDisplay(m_sim, this);
    connect(m_sim, &Simulation::updated, m_radar, &RadarDisplay::onSimUpdated);
    auto* radarDock = new QDockWidget("AN/MPQ-65 RADAR", this);
    radarDock->setObjectName("radarDock");
    radarDock->setWidget(m_radar);
    radarDock->setMinimumWidth(240);
    radarDock->setMaximumWidth(420);
    addDockWidget(Qt::LeftDockWidgetArea, radarDock);

    // Bottom dock — event log
    auto* bottomDock = new QDockWidget("EVENT LOG", this);
    bottomDock->setObjectName("bottomDock");
    m_log = new QTextEdit;
    m_log->setObjectName("eventLog");
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(100);
    m_log->setMaximumHeight(180);
    bottomDock->setWidget(m_log);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
}

// ─── Slots ───────────────────────────────────────────────────

Simulation::LaunchParams MainWindow::collectParams() const
{
    Simulation::LaunchParams p;
    p.speed     = float(m_sbSpeed->value());
    p.elevation = float(m_sbElev->value());
    p.azimuth   = float(m_sbAz->value());
    p.windSpeed = float(m_sbWind->value());
    p.windDir   = float(m_sbWindDir->value());
    p.latitude    = m_cbCoriolis->isChecked() ? float(m_sbLat->value()) : 0.f;
    p.mass        = float(m_sbMass->value());
    p.diameter    = float(m_sbDiam->value());
    p.maneuvering = m_cbManeuver && m_cbManeuver->isChecked();
    return p;
}

void MainWindow::onLaunchTarget()
{
    m_sim->setParams(collectParams());
    if(!m_pendingImpact.isNull())
        m_sim->launchTarget(m_pendingImpact);
    else
        m_sim->launchTarget();
    m_pendingImpact = {};
}

void MainWindow::onReset()
{
    m_sim->reset();
    m_log->clear();
    m_pendingImpact = {};
    if(m_btnPause) m_btnPause->setText("⏸ PAUSE");
}

void MainWindow::onGroundClicked(QVector3D wp)
{
    if(!m_cbClickMode || !m_cbClickMode->isChecked()) return;
    m_pendingImpact = wp;

    // Auto-derive azimuth from clicked point
    float az = qRadiansToDegrees(qAtan2(wp.x(), wp.y()));
    if(az < 0) az += 360.f;
    {
        QSignalBlocker b1(m_slAz), b2(m_sbAz);
        m_sbAz->setValue(az);
        m_slAz->setValue(int(az));
    }

    // Launch immediately
    m_sim->setParams(collectParams());
    m_sim->launchTarget(wp);
    m_pendingImpact = {};
}

void MainWindow::onSimUpdated()
{
    // Status indicators
    setIndicator(m_lblRadar,    m_sim->hasRadarContact());
    setIndicator(m_lblTracking, m_sim->hasActiveInterceptors());
    setIndicator(m_lblWeapons,  m_sim->hasActiveInterceptors());

    // Sim time MM:SS.t
    float t  = m_sim->elapsedTime();
    int   mm = int(t) / 60;
    int   ss = int(t) % 60;
    int   tt = int(t * 10.f) % 10;
    m_lblSimTime->setText(QString("SIM TIME  %1:%2.%3")
                          .arg(mm,2,10,QChar('0'))
                          .arg(ss,2,10,QChar('0'))
                          .arg(tt));
}

void MainWindow::setIndicator(QLabel* lbl, bool active)
{
    const char* prop = active ? "active" : "inactive";
    if(lbl->property("indicator").toString() != prop){
        lbl->setProperty("indicator", prop);
        lbl->style()->unpolish(lbl);
        lbl->style()->polish(lbl);
    }
}

void MainWindow::logEvent(const QString& msg)
{
    // Colorize by prefix
    QString color = "#8A9AB8"; // default gray-blue
    bool bold = false;

    if(msg.startsWith("RADAR"))     color = "#FFB020";
    else if(msg.startsWith("PAC-3") || msg.startsWith("AUTO")) color = "#00FF88";
    else if(msg.startsWith("INTERCEPT")){ color = "#00FF88"; bold = true; }
    else if(msg.startsWith("TARGET"))   color = "#5A8ABF";
    else if(msg.startsWith("IMPACT"))   { color = "#FF3B30"; bold = true; }

    // Timestamp
    QString ts = QTime::currentTime().toString("HH:mm:ss");
    QString html = QString(
        "<span style='color:#3A4A5A;'>[%1]</span> "
        "<span style='color:%2;%3'>%4</span>")
        .arg(ts, color,
             bold?"font-weight:bold;":"",
             msg.toHtmlEscaped());

    m_log->append(html);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}
