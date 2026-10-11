#include "qml/CameraBackend.hpp"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

/// Qt client entry point.
///
/// Phase 1: creates the window, loads the QML module, and exposes the backend
/// as the `backend` context property. The backend has no data source yet.
/// Phase 4 gives it a ServerClient (HTTP control) and StreamPlayer (RTSP), and
/// starts the ~500 ms refresh timer.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CameraBackend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.loadFromModule("CameraPlatform", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }

    return app.exec();
}
