#pragma once

#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "common/Protocol.hpp"
#include "video/VideoPipeline.hpp"

class ProfileHost;

/// RTSP transport for the encoded buffers produced by ProfileHost.
///
/// One shared GstRTSPServer owns one shared media factory per profile. Mount
/// names come from ProfileHost::profiles(); encoded buffers come from the
/// matching borrowed appsink. The host must outlive this server. Detach before
/// destroying the host or the attached GMainContext.
class RtspProfileServer
{
public:
    explicit RtspProfileServer(ProfileHost &host,
                               std::uint16_t port = proto::kRtspPort);
    ~RtspProfileServer();

    RtspProfileServer(const RtspProfileServer &) = delete;
    RtspProfileServer &operator=(const RtspProfileServer &) = delete;
    RtspProfileServer(RtspProfileServer &&) = delete;
    RtspProfileServer &operator=(RtspProfileServer &&) = delete;

    /// Adds mounts and attaches to context. A second attach fails; detach is
    /// safe to call more than once.
    PipelineOutcome attach(GMainContext &context);

    /// Removes the server from its context and releases all RTSP objects.
    void detach();

private:
    struct Mount;

    ProfileHost &m_host;
    std::uint16_t m_port;
    GstRTSPServer *m_server{nullptr};
    std::vector<std::unique_ptr<Mount>> m_mounts;
    GMainContext *m_context{nullptr}; // owns a ref while the server is attached
    guint m_attachId{0};

    static GstRTSPMediaFactory *createFactory(Mount *mount);
    static void onMediaConfigure(GstRTSPMediaFactory *factory,
                                 GstRTSPMedia *media,
                                 gpointer userData);
    static void onMediaPrepared(GstRTSPMedia *media, gpointer userData);
    static void onMediaUnprepared(GstRTSPMedia *media, gpointer userData);
    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData);
};
