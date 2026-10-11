#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <gst/gst.h>

/// Delivery protocol for the remote (cloud) destination.
/// README Phase 8 (lines 357-371): the protocol is chosen only after local
/// streaming works, so this enum is intentionally a superset for now.
enum class CloudProtocol
{
    WebRTC,
    RTMP,
    SRT,
    HLS
};

inline const char *toString(CloudProtocol protocol)
{
    switch (protocol)
    {
    case CloudProtocol::WebRTC:
        return "webrtc";
    case CloudProtocol::RTMP:
        return "rtmp";
    case CloudProtocol::SRT:
        return "srt";
    case CloudProtocol::HLS:
        return "hls";
    }
    return "unknown";
}

/// Everything needed to reach a cloud ingest endpoint.
struct CloudConfig
{
    CloudProtocol protocol{CloudProtocol::WebRTC};
    std::string endpoint;   ///< e.g. "wss://ingest.example.com/live"
    std::string streamKey;  ///< secret credential, never logged
    std::uint32_t bitrateKbps{2000};
};

/// Pushes the encoded stream to a remote backend.
///
/// STATUS: interface only. CloudStreamer.cpp is not implemented — README
/// Phase 8. configure() should validate and store the config; connect() should
/// return false until a real backend is selected.
class CloudStreamer
{
public:
    CloudStreamer() = default;
    ~CloudStreamer() = default;

    bool configure(const CloudConfig &config);
    bool connect();
    void disconnect();
    bool isConnected() const;
    CloudConfig config() const;

    void setOnStateChanged(std::function<void(bool connected)> callback);

private:
    CloudConfig m_config;
    bool m_connected{false};
    GstElement *m_sink{nullptr};
    std::function<void(bool)> m_onStateChanged;

    CloudStreamer(const CloudStreamer &) = delete;
    CloudStreamer &operator=(const CloudStreamer &) = delete;
    CloudStreamer(CloudStreamer &&) = delete;
    CloudStreamer &operator=(CloudStreamer &&) = delete;
};
