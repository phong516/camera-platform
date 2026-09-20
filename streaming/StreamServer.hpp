#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include <gst/gst.h>

/// Transport used to deliver the encoded stream to another machine.
enum class StreamProtocol
{
    RTSP,
    RTMP,
    SRT,
    WebRTC
};

inline const char *toString(StreamProtocol protocol)
{
    switch (protocol)
    {
    case StreamProtocol::RTSP:
        return "rtsp";
    case StreamProtocol::RTMP:
        return "rtmp";
    case StreamProtocol::SRT:
        return "srt";
    case StreamProtocol::WebRTC:
        return "webrtc";
    }
    return "unknown";
}

/// Serves the pipeline output over the network.
///
/// STATUS: interface only. StreamServer.cpp is not implemented yet — README
/// Phase 4 (network streaming) and Phase 7 (tee to multiple outputs).
///
/// Implementation notes:
///   - start(): build the transport sink (RTSP via gst-rtsp-server, or
///     tcpserversink/udpsink as a stepping stone) and begin accepting clients
///   - attachToPipeline(): Phase 7 — request a tee pad on the pipeline and link
///     an encoded branch, so video is encoded exactly once (README lines 327-355)
///   - stop(): tear the sink down and reset m_running; safe to call when stopped
///   - url(): human-readable address, e.g. "rtsp://192.168.1.20:8554/live"
class StreamServer
{
public:
    StreamServer() = default;
    ~StreamServer() = default;

    bool start(StreamProtocol protocol, std::uint16_t port);
    void stop();
    bool isRunning() const;
    StreamProtocol protocol() const;
    std::uint16_t port() const;
    std::string url() const;

    /// Phase 7: link an encoded branch off the pipeline's tee.
    bool attachToPipeline(GstElement *pipeline);
    void detach();

    void setOnClientCountChanged(std::function<void(int)> callback);

private:
    std::atomic<bool> m_running{false};
    StreamProtocol m_protocol{StreamProtocol::RTSP};
    std::uint16_t m_port{0};
    GstElement *m_sink{nullptr};
    std::function<void(int)> m_onClientCountChanged;

    StreamServer(const StreamServer &) = delete;
    StreamServer &operator=(const StreamServer &) = delete;
    StreamServer(StreamServer &&) = delete;
    StreamServer &operator=(StreamServer &&) = delete;
};
