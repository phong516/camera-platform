#pragma once

#include <gst/gst.h>
#include <functional>
#include <memory>
#include <string>
#include "VideoSource.hpp"

struct VideoCaps
{
    const gchar *format{"RGB"};
    gint width{640};
    gint height{480};
    gint fps_num{30};
    gint fps_denom{1};

    /// Builds the "video/x-raw" caps described by these values.
    /// TODO(VideoPipeline.cpp): define this, then call it from both
    /// setupPipeline() and setCapsProperty() so caps live in one place.
    /// Caller owns the result and must gst_caps_unref() it.
    GstCaps *toGstCaps() const;
};

/// A bus message reduced to a plain C++ value, so Application never touches
/// GstMessage, GstBus, or GError. Keeps the layering: GStreamer knowledge
/// stops at VideoPipeline (README lines 110-119).
enum class PipelineEvent
{
    Error,
    Warning,
    EndOfStream
};

inline const char *toString(PipelineEvent event)
{
    switch (event)
    {
    case PipelineEvent::Error:
        return "error";
    case PipelineEvent::Warning:
        return "warning";
    case PipelineEvent::EndOfStream:
        return "end-of-stream";
    }
    return "unknown";
}

/// Invoked on the GLib main context, i.e. the Application main thread.
using PipelineEventCallback = std::function<void(PipelineEvent event, const std::string &message)>;

class VideoPipeline
{
public:
    VideoPipeline() = default;
    ~VideoPipeline();

    bool setVideoSource(std::unique_ptr<VideoSource> source);
    bool setupPipeline();
    bool playPipeline();
    bool pausePipeline();
    bool stopPipeline();
    bool seekPipeline(gint64 position);
    bool setCapsProperty(const VideoCaps &caps);
    gint64 getCurrentPosition() const;
    gint64 getDuration() const;

    /// The underlying GstPipeline, or nullptr before setupPipeline().
    /// Phase 7 (README lines 327-355) adds a tee branch through this handle.
    GstElement *pipelineElement() const;

    /// PUSH: installs a gst_bus_add_watch() source on the default GMainContext
    /// and reports every ERROR/WARNING/EOS through the registered callback.
    /// VideoPipeline owns the bus; Application only owns the GMainLoop.
    /// TODO(VideoPipeline.cpp): implement these three, and call detachBusWatch()
    /// from cleanup() BEFORE the pipeline is torn down so no callback fires
    /// during teardown.
    bool attachBusWatch();
    void detachBusWatch();
    void setOnPipelineEvent(PipelineEventCallback callback);

    /// PULL: non-blocking bus drain for a caller that ticks it (a
    /// g_timeout_add in Application, or a test). Stores the last ERROR/EOS
    /// text in m_lastError; never clears it on an empty poll.
    bool pollBus();

    /// Last bus error text, empty when healthy.
    std::string lastError() const;

private:
    std::unique_ptr<VideoSource> m_videoSource{nullptr};

    GstElement *m_source{nullptr};
    GstElement *m_pipeline{nullptr};
    GstElement *m_videoConvert{nullptr};
    GstElement *m_videoCaps{nullptr};
    GstElement *m_videoSink{nullptr};
    VideoCaps m_caps;
    std::string m_lastError;
    guint m_busWatchId{0};
    PipelineEventCallback m_onPipelineEvent;

    VideoPipeline(const VideoPipeline &) = delete;
    VideoPipeline &operator=(const VideoPipeline &) = delete;
    VideoPipeline(VideoPipeline &&) = delete;
    VideoPipeline &operator=(VideoPipeline &&) = delete;

    bool isPipelineSetup() const;
    template <typename... T>
    bool isPipelineInState(T... state) const;
    bool cleanup();
    bool cleanupSubElements();

    static gboolean onBusMessageStatic(GstBus *bus, GstMessage *message, gpointer userData);
    void handleBusMessage(GstMessage *message);
};