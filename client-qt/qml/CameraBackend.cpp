#include "CameraBackend.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace
{
QString formatResolution(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return {};
    }
    return QStringLiteral("%1x%2").arg(width).arg(height);
}
} // namespace

CameraBackend::CameraBackend(QObject *parent)
    : QObject(parent)
{
}

CameraBackend::~CameraBackend() = default;

QString CameraBackend::statusText() const
{
    const QString state = m_current.value(QStringLiteral("state")).toString();
    return state.isEmpty() ? QStringLiteral("unknown") : state;
}

QString CameraBackend::resolution() const
{
    return formatResolution(m_current.value(QStringLiteral("width")).toInt(),
                            m_current.value(QStringLiteral("height")).toInt());
}

int CameraBackend::fps() const
{
    return m_current.value(QStringLiteral("fps")).toInt();
}

bool CameraBackend::streaming() const
{
    return m_streaming;
}

double CameraBackend::bitrateKbps() const
{
    return m_current.value(QStringLiteral("bitrateKbps")).toDouble();
}

double CameraBackend::cpuPercent() const
{
    return m_current.value(QStringLiteral("cpuPercent")).toDouble();
}

QString CameraBackend::lastError() const
{
    return m_lastError;
}

QVariantList CameraBackend::profiles() const
{
    return m_profiles;
}

bool CameraBackend::applySnapshot(const QByteArray &json)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        m_lastError = QStringLiteral("invalid control API payload: %1").arg(parseError.errorString());
        emit errorOccurred();
        return false;
    }

    const QJsonArray array = doc.object().value(QStringLiteral("profiles")).toArray();
    if (array.isEmpty())
    {
        m_lastError = QStringLiteral("control API returned no profiles");
        emit errorOccurred();
        return false;
    }

    m_profiles = array.toVariantList();

    // The profile the UI is showing: keep the current one if it still exists,
    // otherwise fall back to the first.
    const QString wanted = m_current.value(QStringLiteral("name")).toString();
    QVariantMap selected = m_profiles.first().toMap();
    for (const QVariant &entry : m_profiles)
    {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("name")).toString() == wanted)
        {
            selected = candidate;
            break;
        }
    }

    const bool wasStreaming = m_streaming;
    m_current = selected;
    m_streaming = selected.value(QStringLiteral("state")).toString() != QStringLiteral("off");
    m_lastError.clear();

    emit cameraChanged();
    emit statsChanged();
    if (wasStreaming != m_streaming)
    {
        emit cameraChanged();
    }
    return true;
}

void CameraBackend::refresh()
{
    // Phase 4: issue GET /api/profiles through ServerClient and hand the body to
    // applySnapshot(). Until then there is nothing to poll.
}

void CameraBackend::play(const QString &profile)
{
    Q_UNUSED(profile)
    // Phase 4: POST /api/profiles/{profile}/on, then point StreamPlayer at the
    // RTSP URL from the refreshed snapshot.
}

void CameraBackend::stop()
{
    // Phase 4: POST /api/profiles/{current}/off.
}

