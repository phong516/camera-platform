#include <QApplication>
#include <QQmlApplicationEngine>

/// Qt client entry point.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.loadFromModule("CameraPlatform", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }

    return app.exec();
}
