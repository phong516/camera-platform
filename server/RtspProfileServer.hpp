#pragma once

#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "common/Protocol.hpp"
#include "model/Profile.hpp"
#include "video/VideoPipeline.hpp"

class ProfileHost;

/// RTSP transport for the encoded buffers produced by ProfileHost.
///
/// One shared GstRTSPServer owns one shared media factory per profile. Each
/// factory exposes appsrc -> h264parse -> rtph264pay and receives encoded
/// buffers from the matching ProfileBin appsink. Viewer lifecycle callbacks
/// control the profile valve without changing the shared pipeline state.
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

    /// Adds the profile mounts and attaches the RTSP server to context.
    PipelineOutcome attach(GMainContext *context);

    /// Removes the server from its context and releases all RTSP objects.
    void detach();

    GstRTSPServer *server() const;

private:
    struct Mount;

    ProfileHost &m_host;
    std::uint16_t m_port;
    GstRTSPServer *m_server{nullptr};
    std::vector<std::unique_ptr<Mount>> m_mounts;
    guint m_attachId{0};

    static GstRTSPMediaFactory *createFactory(Mount *mount);
    static void onMediaConfigure(GstRTSPMediaFactory *factory,
                                 GstRTSPMedia *media,
                                 gpointer userData);
    static void onMediaPrepared(GstRTSPMedia *media, gpointer userData);
    static void onMediaUnprepared(GstRTSPMedia *media, gpointer userData);
    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData);
};
