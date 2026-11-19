#include "CameraBackend.hpp"
#include "Protocol.hpp"
#include <QNetworkReply>
#include <qcontainerfwd.h>
#include <qjsondocument.h>
#include <qlogging.h>
#include <qmutex.h>
#include <qnetworkrequest.h>
#include <qurl.h>

CameraBackend::CameraBackend(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &CameraBackend::refresh);
    refresh();
    m_timer.start();
}

CameraBackend::~CameraBackend()
{
}
QString CameraBackend::statusText() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["state"].toString();
        }
    }
    return proto::kStateError;
}
QString CameraBackend::resolution() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["width"].toString() + QString::fromLatin1("x") + profile["height"].toString();
        }
    }
    return proto::kStateError;
}
int CameraBackend::fps() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["fps"].toInt();
        }
    }
    return 0;
}
bool CameraBackend::streaming() const
{

    QMutexLocker<QMutex> lock(&m_mutex);
    return m_streaming;
}

double CameraBackend::bitrateKbps() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["bitrateKbps"].toDouble();
        }
    }
    return 0.0;
}
double CameraBackend::cpuPercent() const
{
    QMutexLocker<QMutex> lock(&m_mutex);

    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["cpuPercent"].toDouble();
        }
    }
    return 0.0;
}
QString CameraBackend::lastError() const
{
    QMutexLocker<QMutex> lock(&m_mutex);

    return m_lastError;
}

bool CameraBackend::applySnapshot(const QByteArray &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isNull() || !doc.isObject())
    {
        m_lastError = "Failed to parse json";
        emit errorOccurred();
        return false;
    }
    if (!doc.toVariant().toMap().contains("profiles"))
    {
        m_lastError = "Json does not have key profiles";
        emit errorOccurred();
        return false;
    }

    QVariantList newProfiles = doc.toVariant().toMap()["profiles"].toList();
    if (newProfiles != m_profiles)
    {
        m_profiles = newProfiles;
        emit profilesChanged();
    }
    return true;
}
void CameraBackend::refresh()
{
    QMutexLocker<QMutex> lock(&m_mutex);

    const QUrl url(QStringLiteral("http://localhost:") + QString::number(proto::kControlPort) +
                   QString::fromLatin1(proto::kApiProfiles));
    QNetworkReply *reply = m_network.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError)
        {
            m_lastError = reply->errorString();
            emit errorOccurred();
        }
        else
        {
            applySnapshot(reply->readAll());
        }
        reply->deleteLater();
    });
}
void CameraBackend::play(const QString &profileName)
{
    QMutexLocker<QMutex> lock(&m_mutex);

    if (profileName.isEmpty())
    {
        m_lastError = "No profile name provided";
        emit errorOccurred();
        return;
    }

    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == profileName)
        {
            m_streaming = true;
            m_selectedProfile = profileName;
            const QUrl url(QStringLiteral("http://localhost:") + QString::number(proto::kControlPort) +
                           QString::fromLatin1(proto::kApiProfiles) + QStringLiteral("/") + profileName +
                           QString::fromLatin1(proto::kActionOn));
            QNetworkReply *reply = m_network.post(QNetworkRequest(url), nullptr);
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                if (reply->error() != QNetworkReply::NoError)
                {
                    m_lastError = reply->errorString();
                    emit errorOccurred();
                }
                else
                {
                    applySnapshot(reply->readAll());
                }
                reply->deleteLater();
            });
        }
    }
    m_lastError = "No profile found";
    emit errorOccurred();
}
void CameraBackend::stop()
{
    QMutexLocker<QMutex> lock(&m_mutex);

    if (m_selectedProfile.isEmpty())
    {
        return;
    }

    m_streaming = false;
    const QUrl url(QStringLiteral("http://localhost:") + QString::number(proto::kControlPort) +
                   QString::fromLatin1(proto::kApiProfiles) + QStringLiteral("/") + m_selectedProfile +
                   QString::fromLatin1(proto::kActionOff));
    QNetworkReply *reply = m_network.post(QNetworkRequest(url), nullptr);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError)
        {
            m_lastError = reply->errorString();
            emit errorOccurred();
        }
        else
        {
            applySnapshot(reply->readAll());
        }
        reply->deleteLater();
    });
}
QVariantList CameraBackend::profiles() const
{
    QMutexLocker<QMutex> lock(&m_mutex);

    return m_profiles;
}
