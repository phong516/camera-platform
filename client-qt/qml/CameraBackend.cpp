#include "CameraBackend.hpp"
#include "Protocol.hpp"
#include <QNetworkReply>
#include <qjsondocument.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qobject.h>
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
    for (const auto &item : m_profiles)
    {
        const QVariantMap &profile = item.toMap();
        if (profile["name"].toString() == m_selectedProfile)
        {
            return profile["state"].toString() == proto::kStateLive;
        }
    }
    return false;
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
    const QUrl url(QStringLiteral("http://localhost:8080/api/profiles"));
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
void CameraBackend::play(const QString &profile)
{
    m_streaming = true;
    m_selectedProfile = profile;
}
void CameraBackend::stop()
{
    m_streaming = false;
}
QVariantList CameraBackend::profiles() const
{
    return m_profiles;
}
