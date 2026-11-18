#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "client-qt/qml/CameraBackend.hpp"
#include "common/Protocol.hpp"
#include "tests/TestSupport.hpp"

/// Qt client test: the QML-visible contract of CameraBackend, without a server
/// or a QML engine. applySnapshot() is the documented single entry point for
/// the Phase 4 HTTP client and for tests, so the wire shape below is the real
/// GET /api/profiles response (see model/Profile.hpp).
///
/// Not covered here: refresh() costs a live HTTP round trip and play()/stop()
/// are specified to call the control API - both need a fake server and a Qt
/// event loop, which is a separate test.

namespace
{
QByteArray snapshot(const char *profiles)
{
    return QByteArray("{\"profiles\":[") + profiles + "]}";
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    CameraBackend backend;

    // --- defaults before any snapshot ---------------------------------------
    CHECK_EQ(backend.fps(), 0);
    CHECK_EQ(backend.bitrateKbps(), 0.0);
    CHECK_EQ(backend.cpuPercent(), 0.0);
    CHECK(!backend.streaming());
    CHECK(backend.lastError().isEmpty());
    CHECK(backend.profiles().isEmpty());

    // --- the real GET /api/profiles wire shape ------------------------------
    const QByteArray twoProfiles = snapshot("{\"name\":\"hd\",\"width\":1920,\"height\":1080,\"fps\":30,"
                                            "\"bitrateKbps\":4000,\"state\":\"ready\","
                                            "\"url\":\"rtsp://127.0.0.1:8554/hd\",\"viewers\":0,"
                                            "\"cpuPercent\":12.5,\"lastError\":\"\"},"
                                            "{\"name\":\"low\",\"width\":854,\"height\":480,\"fps\":15,"
                                            "\"bitrateKbps\":800,\"state\":\"live\","
                                            "\"url\":\"rtsp://127.0.0.1:8554/low\",\"viewers\":1,"
                                            "\"cpuPercent\":3.25,\"lastError\":\"\"}");

    CHECK(backend.applySnapshot(twoProfiles));
    CHECK(backend.lastError().isEmpty());

    const QVariantList parsed = backend.profiles();
    CHECK_EQ(parsed.size(), 2);
    CHECK(parsed[0].toMap()["name"].toString() == QStringLiteral("hd"));
    CHECK(parsed[1].toMap()["name"].toString() == QStringLiteral("low"));

    // --- play() selects the profile the read-only properties project --------
    backend.play(QStringLiteral("hd"));
    CHECK(backend.streaming());
    CHECK(backend.statusText() == QString(proto::kStateReady));
    CHECK(backend.resolution() == QStringLiteral("1920x1080"));
    CHECK_EQ(backend.fps(), 30);
    CHECK_EQ(backend.bitrateKbps(), 4000.0);
    CHECK_EQ(backend.cpuPercent(), 12.5);

    backend.play(QStringLiteral("low"));
    CHECK(backend.streaming());
    CHECK(backend.statusText() == QString(proto::kStateLive));
    CHECK(backend.resolution() == QStringLiteral("854x480"));
    CHECK_EQ(backend.fps(), 15);
    CHECK_EQ(backend.bitrateKbps(), 800.0);
    CHECK_EQ(backend.cpuPercent(), 3.25);

    backend.stop();
    CHECK(!backend.streaming());
    backend.stop(); // idempotent
    CHECK(!backend.streaming());

    // --- unusable payloads are data, not crashes ----------------------------
    // QSignalSpy would drag in Qt6::Test; a plain counter is enough to prove
    // that a rejected payload reaches QML as errorOccurred.
    int errors = 0;
    QObject::connect(&backend, &CameraBackend::errorOccurred, &backend, [&errors]() { ++errors; });

    CHECK(!backend.applySnapshot(QByteArray("not json")));
    CHECK(!backend.lastError().isEmpty());
    CHECK_EQ(errors, 1);

    CHECK(!backend.applySnapshot(QByteArray("[1,2,3]"))); // valid JSON, wrong shape
    CHECK_EQ(errors, 2);

    CHECK(!backend.applySnapshot(QByteArray("null")));
    CHECK_EQ(errors, 3);

    // A rejected payload never replaces the last good catalog.
    CHECK_EQ(backend.profiles().size(), 2);
    CHECK(backend.profiles()[0].toMap()["name"].toString() == QStringLiteral("hd"));

    // --- a new snapshot replaces the catalog, it does not append ------------
    CHECK(backend.applySnapshot(snapshot("{\"name\":\"thumb\",\"width\":320,\"height\":180,\"fps\":5,"
                                         "\"bitrateKbps\":100,\"state\":\"off\","
                                         "\"url\":\"rtsp://127.0.0.1:8554/thumb\",\"viewers\":0,"
                                         "\"cpuPercent\":0.5,\"lastError\":\"\"}")));
    CHECK_EQ(backend.profiles().size(), 1);
    CHECK(backend.profiles()[0].toMap()["name"].toString() == QStringLiteral("thumb"));

    TEST_RETURN();
}
