#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include "Simulation.h"

class Simulation;

class SimRenderer : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit SimRenderer(Simulation* sim, QWidget* parent = nullptr);
    ~SimRenderer() override;

    // tracking mode: camera follows selected missile
    void setTrackTarget(bool on) { m_trackTarget = on; }
    void resetCamera();
    float camAzimuth() const { return m_camAzimuth; }

signals:
    void groundClicked(QVector3D worldPos);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void buildGroundGrid();
    void buildBatteryMarker();
    void buildCircle(int segments);  // for explosions

    void drawGrid();
    void drawBattery();
    void drawAssets();
    void drawAIPanel(QPainter& p);
    void drawMissiles();
    void drawExplosions();
    void drawHUD(QPainter& p);
    void drawTelemetry(QPainter& p);
    void drawMissionOverlay(QPainter& p);
    void drawCompass(QPainter& p);
    void drawTrackingReticles(QPainter& p);
    void drawBatteryLabels(QPainter& p);

    QVector3D unprojectGround(QPoint screenPt) const;

    // Shaders
    QOpenGLShaderProgram m_basicProg;   // uniform color
    QOpenGLShaderProgram m_trailProg;   // per-vertex alpha

    // Grid VAO/VBO
    GLuint m_gridVAO = 0, m_gridVBO = 0;
    int    m_gridVertCount = 0;

    // Battery VAO/VBO
    GLuint m_batVAO = 0, m_batVBO = 0;
    int    m_batVertCount = 0;

    // Dynamic trail/explosion VBO
    GLuint m_dynVAO = 0, m_dynVBO = 0;

    // Circle template (for explosions)
    GLuint m_circleVAO = 0, m_circleVBO = 0;
    int    m_circleSegments = 64;

    // Camera state
    float  m_camAzimuth  = 30.f;   // degrees (public getter below)
    float  m_camElevation= 45.f;   // degrees
    float  m_camDist     = 80000.f; // meters
    QVector3D m_camTarget= {0,0,0};
    QVector3D m_camPanOffset = {0,0,0};
    bool   m_trackTarget = false;

    QPoint m_lastMousePos;
    QPoint m_pressPos;
    bool   m_lmbDown  = false;
    bool   m_rmbDown  = false;
    bool   m_wasDrag  = false;

    QMatrix4x4 m_proj;
    QMatrix4x4 m_view;
    QMatrix4x4 m_vp;   // view * proj — cached

    Simulation* m_sim;
};
