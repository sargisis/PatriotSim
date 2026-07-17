#include "mainwindow.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QFile>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    // OpenGL 3.3 Core before QApplication
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication a(argc, argv);
    a.setApplicationName("PatriotSim");
    a.setOrganizationName("PatriotSim");

    // Load stylesheet
    QFile qss(":/theme.qss");
    if(qss.open(QFile::ReadOnly))
        a.setStyleSheet(QString::fromUtf8(qss.readAll()));

    // JetBrains Mono (optional — use system font if absent, QSS fallback handles it)
    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");

    // Default monospace font for the app
    QFont appFont("JetBrains Mono,Consolas,Courier New", 12);
    a.setFont(appFont);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
