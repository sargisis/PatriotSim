#include "SimRenderer.h"
#include "Simulation.h"
#include "entities/Missile.h"
#include "entities/ProtectedAsset.h"
#include "GameState.h"
#include "physics/Ballistic.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QVector>
#include <cmath>

static constexpr float PI = 3.14159265f;

// ──────────────────────── shaders ────────────────────────
static const char* BASIC_VERT = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";
static const char* BASIC_FRAG = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";
static const char* TRAIL_VERT = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aAlpha;
uniform mat4 uMVP;
out float vAlpha;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); vAlpha = aAlpha; }
)";
static const char* TRAIL_FRAG = R"(
#version 330 core
in float vAlpha;
uniform vec3 uColor;
out vec4 FragColor;
void main() { FragColor = vec4(uColor, vAlpha); }
)";

// ─────────────────────────────────────────────────────────

SimRenderer::SimRenderer(Simulation* sim, QWidget* parent)
    : QOpenGLWidget(parent), m_sim(sim)
{
    setFocusPolicy(Qt::StrongFocus);
    connect(sim, &Simulation::updated, this, [this]{ update(); });
}

SimRenderer::~SimRenderer()
{
    makeCurrent();
    glDeleteVertexArrays(1, &m_gridVAO);   glDeleteBuffers(1, &m_gridVBO);
    glDeleteVertexArrays(1, &m_batVAO);    glDeleteBuffers(1, &m_batVBO);
    glDeleteVertexArrays(1, &m_dynVAO);    glDeleteBuffers(1, &m_dynVBO);
    glDeleteVertexArrays(1, &m_circleVAO); glDeleteBuffers(1, &m_circleVBO);
    doneCurrent();
}

void SimRenderer::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.04f, 0.055f, 0.08f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.5f);
    glPointSize(6.f);

    m_basicProg.addShaderFromSourceCode(QOpenGLShader::Vertex,   BASIC_VERT);
    m_basicProg.addShaderFromSourceCode(QOpenGLShader::Fragment, BASIC_FRAG);
    m_basicProg.link();

    m_trailProg.addShaderFromSourceCode(QOpenGLShader::Vertex,   TRAIL_VERT);
    m_trailProg.addShaderFromSourceCode(QOpenGLShader::Fragment, TRAIL_FRAG);
    m_trailProg.link();

    buildGroundGrid();
    buildBatteryMarker();
    buildCircle(m_circleSegments);

    glGenVertexArrays(1, &m_dynVAO);
    glGenBuffers(1, &m_dynVBO);
    glBindVertexArray(m_dynVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_dynVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void SimRenderer::buildGroundGrid()
{
    QVector<float> v;
    const float H = 25000.f, S = 1000.f;
    for (float x = -H; x <= H+.5f; x += S) {
        v << x<<-H<<0.f << x<< H<<0.f;
    }
    for (float y = -H; y <= H+.5f; y += S) {
        v <<-H<<y<<0.f <<  H<<y<<0.f;
    }
    m_gridVertCount = v.size() / 3;
    glGenVertexArrays(1,&m_gridVAO); glGenBuffers(1,&m_gridVBO);
    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SimRenderer::buildBatteryMarker()
{
    // Build one battery template centred at origin; draw() translates per battery
    QVector<float> v;
    auto L=[&](QVector3D a,QVector3D b){
        v<<a.x()<<a.y()<<a.z()<<b.x()<<b.y()<<b.z();
    };
    // 3 launch containers
    float offs[][2]={{-200,0},{0,200},{200,0}};
    for(auto& o:offs){
        float x=o[0],y=o[1];
        L({x-60,y,0},{x+60,y,0}); L({x,y-60,0},{x,y+60,0});
        L({x,y,0},{x,y,250});
    }
    // Radar ring
    int seg=32;
    for(int i=0;i<seg;++i){
        float a0=2*PI*i/seg,a1=2*PI*(i+1)/seg;
        L({180*std::cos(a0),180*std::sin(a0),60},{180*std::cos(a1),180*std::sin(a1),60});
    }
    m_batVertCount=v.size()/3;
    glGenVertexArrays(1,&m_batVAO); glGenBuffers(1,&m_batVBO);
    glBindVertexArray(m_batVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_batVBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SimRenderer::buildCircle(int seg)
{
    QVector<float> v;
    for(int i=0;i<=seg;++i){
        float a=2*PI*i/seg;
        v<<std::cos(a)<<std::sin(a)<<0.f;
    }
    glGenVertexArrays(1,&m_circleVAO); glGenBuffers(1,&m_circleVBO);
    glBindVertexArray(m_circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_circleVBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SimRenderer::resizeGL(int w,int h)
{
    m_proj.setToIdentity();
    m_proj.perspective(45.f,float(w)/float(h?h:1),100.f,500000.f);
}

void SimRenderer::resetCamera()
{
    m_camAzimuth=30.f; m_camElevation=45.f; m_camDist=80000.f;
    m_camTarget={}; m_camPanOffset={};
    update();
}

static QVector3D sph2cart(float azDeg,float elDeg,float dist)
{
    float az=qDegreesToRadians(azDeg), el=qDegreesToRadians(elDeg);
    return {dist*std::cos(el)*std::sin(az),
            dist*std::cos(el)*std::cos(az),
            dist*std::sin(el)};
}

void SimRenderer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    if(m_trackTarget)
        for(const auto& m:m_sim->missiles())
            if(m.type==MissileType::Target&&m.state==MissileState::Flying)
            { m_camTarget=m.pos; break; }

    QVector3D eye=m_camTarget+m_camPanOffset+sph2cart(m_camAzimuth,m_camElevation,m_camDist);
    m_view.setToIdentity();
    m_view.lookAt(eye, m_camTarget+m_camPanOffset, {0,0,1});
    m_vp=m_proj*m_view;

    drawGrid();
    drawAssets();
    drawBattery();
    drawMissiles();
    drawExplosions();

    // HUD overlay
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    drawMissionOverlay(p);
    drawTelemetry(p);
    drawTrackingReticles(p);
    drawBatteryLabels(p);
    drawCompass(p);
    p.end();
}

void SimRenderer::drawGrid()
{
    m_basicProg.bind();
    m_basicProg.setUniformValue("uMVP",  m_vp);
    m_basicProg.setUniformValue("uColor",QVector4D(0.11f,0.18f,0.24f,1.f));
    glBindVertexArray(m_gridVAO);
    glDrawArrays(GL_LINES,0,m_gridVertCount);
    glBindVertexArray(0);
}

void SimRenderer::drawBattery()
{
    m_basicProg.bind();
    glBindVertexArray(m_batVAO);

    for(const auto& bty : m_sim->batteries()){
        QMatrix4x4 model;
        model.translate(bty.pos);
        m_basicProg.setUniformValue("uMVP",  m_vp * model);
        m_basicProg.setUniformValue("uColor",QVector4D(0.f,1.f,0.53f,1.f));
        glDrawArrays(GL_LINES,0,m_batVertCount);

        // Connection lines between batteries (thin, dark)
    }

    // Draw battery name labels via QPainter later; for now just the geometry
    // Draw inter-battery lines (defensive perimeter)
    const auto& btys = m_sim->batteries();
    if(btys.size() >= 3){
        QVector<float> perim;
        for(int i=0;i<btys.size();++i){
            const QVector3D& a=btys[i].pos;
            const QVector3D& b=btys[(i+1)%btys.size()].pos;
            perim<<a.x()<<a.y()<<a.z()+50<<b.x()<<b.y()<<b.z()+50;
        }
        glBindVertexArray(m_dynVAO);
        glBindBuffer(GL_ARRAY_BUFFER,m_dynVBO);
        // Pack as (x,y,z,alpha) for the dynamic VAO
        QVector<float> buf;
        for(int i=0;i<perim.size();i+=3)
            buf<<perim[i]<<perim[i+1]<<perim[i+2]<<0.3f;
        glBufferData(GL_ARRAY_BUFFER,buf.size()*sizeof(float),buf.data(),GL_STREAM_DRAW);
        m_trailProg.bind();
        m_trailProg.setUniformValue("uMVP",m_vp);
        m_trailProg.setUniformValue("uColor",QVector3D(0.f,0.8f,0.4f));
        glDrawArrays(GL_LINES,0,buf.size()/4);
    }

    glBindVertexArray(0);
}

void SimRenderer::drawAssets()
{
    const GameState* gs = m_sim->gameState();
    if (!gs || gs->assets().isEmpty()) return;

    m_basicProg.bind();
    glBindVertexArray(m_circleVAO);

    for (const auto& a : gs->assets()) {
        float frac = a.healthFrac();
        // Color: green(full) → yellow(50%) → red(dead)
        float r = (frac < 0.5f) ? 1.f : (1.f - frac) * 2.f;
        float g = (frac > 0.5f) ? 1.f : frac * 2.f;
        float b = 0.f;
        float alpha = a.destroyed() ? 0.25f : 0.6f;

        // Kill radius ring
        QMatrix4x4 model;
        model.translate(QVector3D(a.pos.x(), a.pos.y(), 10.f));
        model.scale(a.killRadius);
        m_basicProg.setUniformValue("uMVP",  m_vp * model);
        m_basicProg.setUniformValue("uColor", QVector4D(r, g, b, alpha * 0.5f));
        glDrawArrays(GL_LINE_STRIP, 0, m_circleSegments + 1);

        // Inner asset marker (small solid ring)
        float markerR = (a.type == ProtectedAsset::CITY)   ? 600.f
                      : (a.type == ProtectedAsset::AIRBASE) ? 350.f
                                                             : 180.f;
        model.setToIdentity();
        model.translate(QVector3D(a.pos.x(), a.pos.y(), 10.f));
        model.scale(markerR);
        m_basicProg.setUniformValue("uMVP",  m_vp * model);
        m_basicProg.setUniformValue("uColor", QVector4D(r, g, b, alpha));
        glDrawArrays(GL_LINE_STRIP, 0, m_circleSegments + 1);
    }
    glBindVertexArray(0);
}

void SimRenderer::drawMissiles()
{
    for(const auto& m:m_sim->missiles()){
        if(m.trail.isEmpty()) continue;

        QVector<float> buf;
        int n=m.trail.size();
        for(int i=0;i<n;++i){
            float a=float(i)/float(n);
            buf<<m.trail[i].x()<<m.trail[i].y()<<m.trail[i].z()<<a;
        }
        buf<<m.pos.x()<<m.pos.y()<<m.pos.z()<<1.f;

        glBindVertexArray(m_dynVAO);
        glBindBuffer(GL_ARRAY_BUFFER,m_dynVBO);
        glBufferData(GL_ARRAY_BUFFER,buf.size()*sizeof(float),buf.data(),GL_STREAM_DRAW);

        // Target = red #FF3B30, interceptor = blue #38B6FF
        QVector3D col=(m.type==MissileType::Target)
            ? QVector3D(1.f,0.23f,0.19f)
            : QVector3D(0.22f,0.71f,1.f);

        m_trailProg.bind();
        m_trailProg.setUniformValue("uMVP",  m_vp);
        m_trailProg.setUniformValue("uColor",col);
        glDrawArrays(GL_LINE_STRIP,0,n+1);

        m_basicProg.bind();
        m_basicProg.setUniformValue("uMVP",m_vp);
        m_basicProg.setUniformValue("uColor",
            m.state==MissileState::Flying
            ? QVector4D(col.x(),col.y(),col.z(),1.f)
            : QVector4D(0.4f,0.4f,0.4f,0.4f));
        glPointSize(m.type==MissileType::Target ? 9.f : 7.f);
        glDrawArrays(GL_POINTS,n,1);
        glBindVertexArray(0);
    }
}

void SimRenderer::drawExplosions()
{
    for(const auto& ex:m_sim->explosions()){
        float t=ex.age/ex.duration;
        float alpha=1.f-t;
        float radius=t*600.f+30.f;

        QMatrix4x4 model;
        model.translate(ex.pos);
        model.scale(radius);

        m_basicProg.bind();
        m_basicProg.setUniformValue("uMVP",  m_vp*model);
        m_basicProg.setUniformValue("uColor",QVector4D(1.f,0.69f,0.13f,alpha)); // amber
        glBindVertexArray(m_circleVAO);
        glDrawArrays(GL_LINE_STRIP,0,m_circleSegments+1);

        // Inner bright flash
        model.setToIdentity();
        model.translate(ex.pos);
        model.scale(radius*0.3f);
        m_basicProg.setUniformValue("uMVP",  m_vp*model);
        m_basicProg.setUniformValue("uColor",QVector4D(1.f,1.f,0.8f,alpha*0.8f));
        glDrawArrays(GL_LINE_STRIP,0,m_circleSegments+1);

        glBindVertexArray(0);
    }
}

// ─────────────────────── HUD ───────────────────────

static QFont hudFont(int px, bool bold=false)
{
    QFont f("JetBrains Mono,Consolas,Courier New");
    f.setPixelSize(px);
    if(bold) f.setBold(true);
    return f;
}

void SimRenderer::drawMissionOverlay(QPainter& p)
{
    const GameState* gs = m_sim->gameState();
    if (!gs || !gs->active()) return;

    const int panW = 210, padX = 14;
    int y = 14;

    // ── Phase / wave banner ──────────────────────────────────
    QString phaseStr;
    QColor  phaseBg(0, 0, 0, 160);
    QColor  phaseFg(0x00, 0xFF, 0x88);

    switch (gs->phase()) {
    case GameState::Phase::COUNTDOWN:
        phaseStr = QString("WAVE %1/%2  INBOUND  T-%3 s")
                   .arg(gs->currentWave()).arg(gs->totalWaves())
                   .arg(int(gs->countdown()) + 1);
        phaseFg = QColor(0xFF, 0xB0, 0x20);
        break;
    case GameState::Phase::WAVE_ACTIVE:
        phaseStr = QString("WAVE %1/%2  ■ ACTIVE")
                   .arg(gs->currentWave()).arg(gs->totalWaves());
        phaseFg = QColor(0xFF, 0x3B, 0x30);
        break;
    case GameState::Phase::INTERWAVE:
        phaseStr = QString("WAVE %1 CLEAR — NEXT WAVE PENDING")
                   .arg(gs->currentWave());
        phaseFg = QColor(0x00, 0xFF, 0x88);
        break;
    case GameState::Phase::MISSION_WIN:
        phaseStr = "MISSION COMPLETE";
        phaseFg  = QColor(0x00, 0xFF, 0x88);
        phaseBg  = QColor(0, 50, 0, 180);
        break;
    case GameState::Phase::MISSION_FAIL:
        phaseStr = "MISSION FAILED";
        phaseFg  = QColor(0xFF, 0x3B, 0x30);
        phaseBg  = QColor(60, 0, 0, 180);
        break;
    default: break;
    }

    if (!phaseStr.isEmpty()) {
        QFont f = hudFont(12, true);
        p.setFont(f);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(phaseStr) + 16;
        QRect banner((width() - tw) / 2, 10, tw, 22);
        p.fillRect(banner, phaseBg);
        p.setPen(QPen(phaseFg, 1));
        p.drawRect(banner);
        p.setPen(phaseFg);
        p.drawText(banner, Qt::AlignCenter, phaseStr);
        y = banner.bottom() + 8;
    }

    // ── Score / stats strip ──────────────────────────────────
    {
        int sw = 200;
        QRect scoreBox(width() - sw - padX, 10, sw, 22);
        p.fillRect(scoreBox, QColor(0, 0, 0, 160));
        p.setPen(QColor(0x38, 0xB6, 0xFF));
        p.setFont(hudFont(11, true));
        p.drawText(scoreBox, Qt::AlignCenter,
                   QString("SCORE %1  |  %2↑ %3↓")
                   .arg(gs->score(), 6)
                   .arg(gs->intercepted())
                   .arg(gs->missed()));
    }

    // ── Asset health bars ────────────────────────────────────
    {
        const auto& assets = gs->assets();
        int bx = padX, by = height() - 14 - assets.size() * 22;
        int bw = 160, bh = 14;

        for (const auto& a : assets) {
            float frac = a.healthFrac();
            float r = (frac < 0.5f) ? 1.f : (1.f - frac) * 2.f;
            float g2 = (frac > 0.5f) ? 1.f : frac * 2.f;

            QRect bg(bx, by, bw, bh);
            p.fillRect(bg, QColor(0, 0, 0, 160));
            p.fillRect(QRect(bx, by, int(bw * frac), bh),
                       QColor(int(r*255), int(g2*255), 0, 200));
            p.setPen(QColor(60, 80, 90));
            p.drawRect(bg);
            p.setFont(hudFont(9, true));
            p.setPen(QColor(220, 220, 220));
            p.drawText(bg.adjusted(4, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1  %2%").arg(a.name).arg(int(a.health)));
            by += bh + 4;
        }
    }
}

void SimRenderer::drawTelemetry(QPainter& p)
{
    const Missile* tgt = nullptr;
    for(const auto& m:m_sim->missiles())
        if(m.type==MissileType::Target&&m.state==MissileState::Flying)
        { tgt=&m; break; }

    if(!tgt) return;

    float alt   = tgt->pos.z();
    float spd   = tgt->vel.length();
    float range = tgt->pos.length();
    float c     = Ballistic::speedOfSound(alt);
    float mach  = spd / (c > 1.f ? c : 1.f);

    // Semi-transparent panel
    QRect box(14, 14, 204, 96);
    p.fillRect(box, QColor(0,0,0,160));
    p.setPen(QPen(QColor(0x00,0xFF,0x88,180), 1));
    p.drawRect(box);

    // Header
    p.setFont(hudFont(10, true));
    p.setPen(QColor(0x38,0xB6,0xFF));
    p.drawText(box.adjusted(6,4,0,0), Qt::AlignTop|Qt::AlignLeft,
               QString("TGT #%1").arg(tgt->id));

    p.setFont(hudFont(11));
    p.setPen(QColor(0x00,0xFF,0x88));

    auto row=[&](int y,const QString& lbl,const QString& val){
        p.setPen(QColor(0x5A,0x8A,0x7A));
        p.drawText(20, y, lbl);
        p.setPen(QColor(0x00,0xFF,0x88));
        p.drawText(80, y, val);
    };

    row(38, "ALT  ", QString("%1 km").arg(alt/1000.f,6,'f',1));
    row(54, "SPD  ", QString("%1 m/s").arg(spd,6,'f',0));
    row(70, "MACH ", QString("%1").arg(mach,6,'f',2));
    row(86, "RANGE", QString("%1 km").arg(range/1000.f,6,'f',1));

    if(tgt->pos.length()<=80000.f){
        p.setFont(hudFont(9,true));
        p.setPen(QColor(0xFF,0xB0,0x20));
        p.drawText(box.right()-58, box.top()+14, "● RADAR LOCK");
    }
}

void SimRenderer::drawCompass(QPainter& p)
{
    const int cx = width()-72, cy = height()-82;
    const int r  = 46;

    // Background
    p.fillRect(cx-r-6, cy-r-6, 2*(r+6), 2*(r+6)+22, QColor(0,0,0,140));

    p.save();
    p.translate(cx, cy);

    // Outer ring
    p.setPen(QPen(QColor(0x1E,0x2A,0x38), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(-r,-r,2*r,2*r);

    // Tick marks
    for(int i=0;i<8;++i){
        float a=i*45.f*PI/180.f;
        float inner=(i%2==0)?r*0.72f:r*0.84f;
        p.setPen(QPen(QColor(i%2==0?0x38:0x22,i%2==0?0xB6:0x3A,i%2==0?0xFF:0x5A),1));
        p.drawLine(QPointF(r*std::sin(a),-r*std::cos(a)),
                   QPointF(inner*std::sin(a),-inner*std::cos(a)));
    }

    // N S E W
    p.setFont(hudFont(9,true));
    auto cardinal=[&](const QString& lbl,float aDeg){
        float a=aDeg*PI/180.f;
        float dr=r+11.f;
        p.setPen(lbl=="N"?QColor(0xFF,0x3B,0x30):QColor(0x38,0xB6,0xFF));
        QPointF pt(dr*std::sin(a)-6.f,-dr*std::cos(a)-5.f);
        p.drawText(QRectF(pt,QSizeF(12,12)),Qt::AlignCenter,lbl);
    };
    cardinal("N",0); cardinal("E",90); cardinal("S",180); cardinal("W",270);

    // Camera-direction needle (points toward current azimuth)
    float azRad=m_camAzimuth*PI/180.f;
    p.setPen(QPen(QColor(0xFF,0x3B,0x30),2));
    p.drawLine(QPointF(0,0),QPointF(r*0.7f*std::sin(azRad),-r*0.7f*std::cos(azRad)));
    p.setPen(QPen(QColor(0x38,0xB6,0xFF),1));
    p.drawLine(QPointF(0,0),QPointF(-r*0.35f*std::sin(azRad),r*0.35f*std::cos(azRad)));

    // Centre dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x38,0xB6,0xFF));
    p.drawEllipse(-3,-3,6,6);

    p.restore();

    // Azimuth text below
    p.setFont(hudFont(10));
    p.setPen(QColor(0x38,0xB6,0xFF));
    int azNorm=int(m_camAzimuth)%360; if(azNorm<0) azNorm+=360;
    p.drawText(cx-20, cy+r+16, QString("%1°").arg(azNorm,3,10,QChar('0')));
}

void SimRenderer::drawBatteryLabels(QPainter& p)
{
    p.setFont(hudFont(10, true));
    for(const auto& bty : m_sim->batteries()){
        QVector4D clip = m_vp * QVector4D(bty.pos + QVector3D(0,0,400), 1.f);
        if(clip.w() <= 0.f) continue;
        float sx = (clip.x()/clip.w()+1.f)*0.5f*width();
        float sy = (1.f-clip.y()/clip.w())*0.5f*height();

        // Color by weapon system
        QColor col = (bty.wsys == WeaponSystem::THAAD)      ? QColor(0x38,0xB6,0xFF)
                   : (bty.wsys == WeaponSystem::IRON_DOME)  ? QColor(0xFF,0xD0,0x40)
                                                             : QColor(0x00,0xFF,0x88);
        p.setPen(col);
        p.drawText(QPointF(sx+6, sy), bty.name);
        p.setFont(hudFont(9));
        p.setPen(QColor(180,180,180));
        p.drawText(QPointF(sx+6, sy+12),
                   QString("%1/%2").arg(bty.ammo).arg(bty.maxAmmo));
        p.setFont(hudFont(10, true));
    }

    // Asset labels
    const GameState* gs = m_sim->gameState();
    if (!gs) return;
    p.setFont(hudFont(9, true));
    for (const auto& a : gs->assets()) {
        QVector4D clip = m_vp * QVector4D(QVector3D(a.pos.x(), a.pos.y(), 800.f), 1.f);
        if (clip.w() <= 0.f) continue;
        float sx = (clip.x()/clip.w()+1.f)*0.5f*width();
        float sy = (1.f-clip.y()/clip.w())*0.5f*height();

        QColor col = a.destroyed() ? QColor(100,100,100)
                   : a.healthFrac() > 0.5f ? QColor(0x40,0xFF,0x80)
                   : a.healthFrac() > 0.25f ? QColor(0xFF,0xB0,0x20)
                                            : QColor(0xFF,0x3B,0x30);
        p.setPen(col);
        p.drawText(QPointF(sx+8, sy), a.name);
        p.setFont(hudFont(8));
        p.setPen(QColor(180,180,180));
        p.drawText(QPointF(sx+8, sy+11), QString("%1%").arg(int(a.health)));
        p.setFont(hudFont(9, true));
    }
}

void SimRenderer::drawTrackingReticles(QPainter& p)
{
    int trkIdx=0;
    for(const auto& m:m_sim->missiles()){
        if(m.type!=MissileType::Target||m.state!=MissileState::Flying){continue;}

        // Project world → screen
        QVector4D clip=m_vp*QVector4D(m.pos,1.0f);
        if(clip.w()<=0.f){trkIdx++;continue;}
        float sx=(clip.x()/clip.w()+1.f)*0.5f*width();
        float sy=(1.f-clip.y()/clip.w())*0.5f*height();

        if(sx<-60||sx>width()+60||sy<-60||sy>height()+60){trkIdx++;continue;}

        bool inRadar=(m.pos.length()<=80000.f);
        QColor col = inRadar ? QColor(0xFF,0xB0,0x20) : QColor(0x8A,0xA0,0xB0);

        // Corner-bracket reticle
        float sz=22.f, arm=9.f;
        QPen pen(col,1.5f);
        p.setPen(pen);
        // TL
        p.drawLine(QPointF(sx-sz,sy-sz),QPointF(sx-sz+arm,sy-sz));
        p.drawLine(QPointF(sx-sz,sy-sz),QPointF(sx-sz,sy-sz+arm));
        // TR
        p.drawLine(QPointF(sx+sz,sy-sz),QPointF(sx+sz-arm,sy-sz));
        p.drawLine(QPointF(sx+sz,sy-sz),QPointF(sx+sz,sy-sz+arm));
        // BL
        p.drawLine(QPointF(sx-sz,sy+sz),QPointF(sx-sz+arm,sy+sz));
        p.drawLine(QPointF(sx-sz,sy+sz),QPointF(sx-sz,sy+sz-arm));
        // BR
        p.drawLine(QPointF(sx+sz,sy+sz),QPointF(sx+sz-arm,sy+sz));
        p.drawLine(QPointF(sx+sz,sy+sz),QPointF(sx+sz,sy+sz-arm));

        // Centre crosshair (small)
        p.setPen(QPen(col,1.f));
        p.drawLine(QPointF(sx-4,sy),QPointF(sx+4,sy));
        p.drawLine(QPointF(sx,sy-4),QPointF(sx,sy+4));

        // Label
        p.setFont(hudFont(10,true));
        p.setPen(col);
        p.drawText(QPointF(sx+sz+4, sy-sz+10),
                   QString("TRK-%1").arg(trkIdx+1,2,10,QChar('0')));

        // Speed vector indicator
        if(m.vel.length()>10.f){
            // Project velocity endpoint
            QVector3D vend=m.pos+m.vel*5.f; // 5-second projection
            QVector4D vclip=m_vp*QVector4D(vend,1.0f);
            if(vclip.w()>0.f){
                float vx=(vclip.x()/vclip.w()+1.f)*0.5f*width();
                float vy=(1.f-vclip.y()/vclip.w())*0.5f*height();
                p.setPen(QPen(QColor(0xFF,0x3B,0x30,160),1));
                p.drawLine(QPointF(sx,sy),QPointF(vx,vy));
            }
        }

        trkIdx++;
    }
}

// ─────────────── input handling ───────────────

void SimRenderer::mousePressEvent(QMouseEvent* e)
{
    m_lastMousePos = e->pos();
    m_pressPos     = e->pos();
    m_wasDrag      = false;
    m_lmbDown = (e->button() == Qt::LeftButton);
    m_rmbDown = (e->button() == Qt::RightButton);
}

void SimRenderer::mouseMoveEvent(QMouseEvent* e)
{
    QPoint d=e->pos()-m_lastMousePos;
    m_lastMousePos=e->pos();
    if((e->pos()-m_pressPos).manhattanLength() > 6)
        m_wasDrag = true;
    if(m_lmbDown){
        m_camAzimuth   -= d.x()*0.4f;
        m_camElevation += d.y()*0.4f;
        m_camElevation  = qBound(5.f,m_camElevation,89.f);
    } else if(m_rmbDown){
        float sc=m_camDist*0.001f;
        QVector3D right=QVector3D::crossProduct(
            m_camTarget-sph2cart(m_camAzimuth,m_camElevation,m_camDist),{0,0,1}).normalized();
        m_camPanOffset += right*(-d.x()*sc)+QVector3D(0,0,1)*(d.y()*sc);
    }
    update();
}

void SimRenderer::mouseReleaseEvent(QMouseEvent* e)
{
    // Only emit groundClicked on a true click (no drag)
    if(e->button()==Qt::LeftButton && !m_wasDrag){
        QVector3D wp=unprojectGround(e->pos());
        if(!wp.isNull()) emit groundClicked(wp);
    }
    m_lmbDown = m_rmbDown = m_wasDrag = false;
}

void SimRenderer::wheelEvent(QWheelEvent* e)
{
    m_camDist *= (e->angleDelta().y()>0)?0.85f:1.18f;
    m_camDist  = qBound(500.f,m_camDist,300000.f);
    update();
}

void SimRenderer::keyPressEvent(QKeyEvent* e)
{
    if(e->key()==Qt::Key_T){ m_trackTarget=!m_trackTarget; update(); }
    if(e->key()==Qt::Key_R){ resetCamera(); }
}

QVector3D SimRenderer::unprojectGround(QPoint sc) const
{
    float ndcX=(2.f*sc.x()/width())-1.f;
    float ndcY=1.f-(2.f*sc.y()/height());
    QMatrix4x4 inv=m_vp.inverted();
    QVector4D n4=inv*QVector4D(ndcX,ndcY,-1.f,1.f);
    QVector4D f4=inv*QVector4D(ndcX,ndcY, 1.f,1.f);
    if(n4.w()==0.f||f4.w()==0.f) return {};
    QVector3D n3=n4.toVector3DAffine(), f3=f4.toVector3DAffine();
    QVector3D dir=f3-n3;
    if(qAbs(dir.z())<1e-6f||dir.z()>0.f) return {};
    float t=-n3.z()/dir.z();
    if(t<0) return {};
    return n3+dir*t;
}
