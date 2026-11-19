#pragma once

// Qt -> QML bridge for the client (README Phase 5).
//
// QML cannot see plain C++ classes, so this is a thin adapter that exposes only
// what the QML files bind to. It deliberately does NOT include server/Application:
// the client talks to the server over HTTP for control and RTSP for video, so it
// never links the server and never links GStreamer.
//
// Phase 1 (now): reads a local JSON snapshot and reports it. Wire a real
//   ServerClient (HTTP GET /api/profiles) into refresh() in Phase 4.
// Phase 4: refresh() polls the control API; play()/stop() call it; StreamPlayer
//   renders the RTSP URL from the selected profile.

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QMutex>
#include <qmutex.h>

class CameraBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY profilesChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY profilesChanged)
    Q_PROPERTY(int fps READ fps NOTIFY profilesChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY cameraChanged)

    Q_PROPERTY(double bitrateKbps READ bitrateKbps NOTIFY profilesChanged)
    Q_PROPERTY(double cpuPercent READ cpuPercent NOTIFY profilesChanged)

    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

  public:
    explicit CameraBackend(QObject *parent = nullptr);
    ~CameraBackend() override;

    QString statusText() const;
    QString resolution() const;
    int fps() const;
    bool streaming() const;

    double bitrateKbps() const;
    double cpuPercent() const;

    QString lastError() const;

    QVariantList profiles() const;

    /// Applies a JSON snapshot of the control API's /api/profiles response.
    /// Returns false and records lastError() when the payload is unusable.
    /// Serve as the single entry point for both the Phase 4 HTTP client and tests.
    bool applySnapshot(const QByteArray &json);

    /// Called by a ~500 ms QTimer; re-reads the backend and emits the signals
    /// whose values actually changed. Push-only - QML never polls.
    void refresh();

    Q_INVOKABLE void play(const QString &profileName = QString());
    Q_INVOKABLE void stop();

  signals:
    void cameraChanged();
    void profilesChanged();
    void errorOccurred();

  private:
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QVariantList m_profiles;
    QString m_selectedProfile;
    QString m_lastError;
    bool m_streaming{false};
    mutable QMutex m_mutex;
};
