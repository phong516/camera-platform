#pragma once

// Qt -> C++ bridge for the QML presentation layer (README Phase 5).
//
// STATUS: prototype, deliberately NOT compiled. Qt6 is not installed on the
// development machine, and `qml/` is still being designed by hand. The whole
// body is therefore behind CAMERA_PLATFORM_WITH_QT, so this header stays
// syntax-clean today and becomes real once Qt6 is available:
//
//     sudo apt install qt6-base-dev qt6-declarative-dev
//     cmake -S . -B build -DCMAKE_CXX_FLAGS=-DCAMERA_PLATFORM_WITH_QT
//
// Why this file must exist: QML cannot see plain C++ classes. Application is
// intentionally not a QObject, so this is a thin adapter (composition, not
// inheritance) that exposes only what QML binds to. See qml/UI-PROTOTYPE.md for
// the wireframe and the property/slot contract the QML files rely on.
//
// Registration in main() (Phase 5), once Qt is enabled:
//     CameraBackend backend(application);
//     engine.rootContext()->setContextProperty("backend", &backend);

#if defined(CAMERA_PLATFORM_WITH_QT)

#include <QObject>
#include <QString>
#include <QVariantList>

#include "app/Application.hpp"

class CameraBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString statusText READ statusText NOTIFY cameraChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY cameraChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY cameraChanged)
    Q_PROPERTY(QString pixelFormat READ pixelFormat NOTIFY cameraChanged)
    Q_PROPERTY(int fps READ fps NOTIFY cameraChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY cameraChanged)

    Q_PROPERTY(double bitrateKbps READ bitrateKbps NOTIFY statsChanged)
    Q_PROPERTY(double latencyMs READ latencyMs NOTIFY statsChanged)
    Q_PROPERTY(double cpuPercent READ cpuPercent NOTIFY statsChanged)

    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    explicit CameraBackend(Application &application, QObject *parent = nullptr);
    ~CameraBackend() override;

    QString statusText() const;
    QString sourceName() const;
    QString resolution() const;
    QString pixelFormat() const;
    int fps() const;
    bool streaming() const;

    double bitrateKbps() const;
    double latencyMs() const;
    double cpuPercent() const;

    QString lastError() const;

    /// Called by a ~500 ms QTimer; re-reads Application and emits the signals
    /// whose values actually changed. Push-only — QML never polls.
    void refresh();

    Q_INVOKABLE void startCamera();
    Q_INVOKABLE void stopCamera();
    Q_INVOKABLE void setResolution(int width, int height);
    Q_INVOKABLE void setFrameRate(int fps);
    Q_INVOKABLE QVariantList availableResolutions() const;
    Q_INVOKABLE QVariantList availableSources() const;

signals:
    void cameraChanged();
    void statsChanged();
    void errorOccurred();

private:
    Application &m_application;
    double m_latencyMs{0.0};
};

#endif // CAMERA_PLATFORM_WITH_QT
