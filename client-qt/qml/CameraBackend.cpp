#include "CameraBackend.hpp"
#include "Protocol.hpp"
#include <QNetworkReply>
#include <qjsondocument.h>
#include <qnetworkrequest.h>
#include <qurl.h>

CameraBackend::CameraBackend(QObject *parent)
{
}

CameraBackend::~CameraBackend()
{
}
QString CameraBackend::statusText() const
{
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
    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["width"].toString() + "x" + profile["height"].toString();
        }
    }
    return proto::kStateError;
}
int CameraBackend::fps() const
{
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
    return m_streaming;
}

double CameraBackend::bitrateKbps() const
{
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
    m_profiles = doc.toVariant().toMap()["profiles"].toList();
    return true;
}
void CameraBackend::refresh()
{
    const QUrl url(QStringLiteral("http://localhost:") + QString(proto::kControlPort) + QString(proto::kApiProfiles));
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
            const QUrl url(QStringLiteral("http://localhost:") + QString(proto::kControlPort) +
                           QString(proto::kApiProfiles) + QStringLiteral("/") + profileName +
                           QString(proto::kActionOn));
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
    if (m_selectedProfile.isEmpty())
    {
        return;
    }

    m_streaming = false;
    const QUrl url(QStringLiteral("http://localhost:") + QString(proto::kControlPort) + QString(proto::kApiProfiles) +
                   QStringLiteral("/") + m_selectedProfile + QString(proto::kActionOff));
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
    return m_profiles;
}
