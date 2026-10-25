#pragma once

#include <functional>
#include <memory>
#include <string>

#include <gst/gst.h>

#include "VideoSink.hpp"
#include "VideoSource.hpp"

/// Result of every pipeline operation. The module's SINGLE error channel:
/// `ok` is the judgment, `error` is the reason when false. No stderr side
/// effects, no sentinel error text - callers log from here.
struct PipelineOutcome
{
    bool ok{false};
    std::string error;

    static PipelineOutcome success() { return PipelineOutcome{true, ""}; }
    static PipelineOutcome failure(std::string reason) { return PipelineOutcome{false, std::move(reason)}; }
};

/// The resolution/format profile applied to the capsfilter.
struct VideoCaps
{
    std::string format{"RGB"};
    gint width{640};
    gint height{480};
    gint fps_num{30};
    gint fps_denom{1};

    /// The ONE place that builds "video/x-raw" caps from these values
    /// (setupPipeline() and setCaps() both go through it; nothing else
    /// builds caps). Caller owns the result and must gst_caps_unref() it.
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

/// Invoked on the GMainContext passed to attachBusWatch(), i.e. the caller's
/// main thread.
using PipelineEventCallback = std::function<void(PipelineEvent event, const std::string &message)>;

/// The whole local capture pipeline: source -> convert -> capsfilter -> sink,
/// the bus (push events), and the playback controls. Everything a caller must
/// know is the method set below plus one lifecycle:
///
///   setVideoSource() [-> setVideoSink() / setCaps() (both optional, any
///   order, before setup)] -> setupPipeline() -> play()/pause()/seek()/setCaps()
///   -> stop() [tears down; setupPipeline() may then rebuild from scratch]
///
/// attachBusWatch() is usable once the pipeline is set up; the callback fires
/// on the GMainContext given there. detachBusWatch() is covered by stop() and
/// the destructor, so no callback can fire during teardown.
///
/// A caller never touches GstMessage, GstBus, or a raw GstElement: even Phase
/// 2's tee + one valve-gated branch per profile stays behind this interface
/// (branch management becomes methods here, not a released handle).
class VideoPipeline
{
public:
    VideoPipeline();
    ~VideoPipeline();

    /// Sets (or replaces) the source factory. Callable any time; re-setup
    /// (after stop()) uses whatever source is current - VideoPipeline holds
    /// the factory, not the element, so stop()+setupPipeline() rebuilds the
    /// same source unless it was replaced.
    PipelineOutcome setVideoSource(std::unique_ptr<VideoSource> source);

    /// Sets the sink factory (video/VideoSink.hpp). Optional - the default is
    /// AutoVideoSink - but must come before setupPipeline() to matter.
    PipelineOutcome setVideoSink(std::unique_ptr<VideoSink> sink);

    /// Stores caps for the next setupPipeline() AND, when the pipeline is
    /// already built, applies them live to the capsfilter.
    PipelineOutcome setCaps(const VideoCaps &caps);

    /// Builds source + convert + capsfilter + sink and links them. Fails on a
    /// second call while set up, or with no source: check error for the reason.
    PipelineOutcome setupPipeline();

    PipelineOutcome play();
    PipelineOutcome pause();

    /// Tears the pipeline down (state NULL, every element cleared). Also runs
    /// in the destructor, and includes detachBusWatch() - the pre-play bus
    /// watch is destroyed before the pipeline is, so no event fires mid-teardown.
    PipelineOutcome stop();

    PipelineOutcome seek(gint64 position);

    /// Playback clock queries, strictly best-effort: -1 when not set up, not
    /// PLAYING/PAUSED, or the query failed. Documented sentinel, not an error.
    gint64 currentPosition() const;
    gint64 duration() const;

    /// PUSH events: installs a bus watch on `context` and reports every
    /// ERROR/WARNING/EOS through `callback` on that context. VideoPipeline
    /// owns the bus; the caller owns the loop that iterates the context.
    /// Requires a set-up pipeline; replaces any previous watch + callback.
    PipelineOutcome attachBusWatch(GMainContext *context, PipelineEventCallback callback);
    void detachBusWatch();

private:
    std::unique_ptr<VideoSource> m_videoSource{nullptr}; // factory, kept across stop()
    std::unique_ptr<VideoSink> m_videoSink{nullptr};     // factory; AutoVideoSink by default

    GstElement *m_source{nullptr};
    GstElement *m_pipeline{nullptr};
    GstElement *m_videoConvert{nullptr};
    GstElement *m_videoCaps{nullptr};
    GstElement *m_sink{nullptr}; // element produced by m_videoSink
    VideoCaps m_caps;

    PipelineEventCallback m_appCallback{nullptr};
    GSource *m_busSource{nullptr};

    using MessageParse = std::function<void(GstMessage*, GError**, gchar**)>;

    VideoPipeline(const VideoPipeline &) = delete;
    VideoPipeline &operator=(const VideoPipeline &) = delete;
    VideoPipeline(VideoPipeline &&) = delete;
    VideoPipeline &operator=(VideoPipeline &&) = delete;

    bool isPipelineSetup() const;
    template <typename... T>
    bool isPipelineInState(T... states) const;

    static gboolean onBusMessageStatic(gpointer userData);
    void handleBusMessage(GstMessage *msg);
    std::string parseMessage(GstMessage *msg, MessageParse parser);
};
