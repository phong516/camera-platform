#pragma once

#include <gst/gst.h>
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
};

class VideoPipeline
{
public:
    VideoPipeline() = default;
    ~VideoPipeline();

    bool setVideoSource(std::unique_ptr<VideoSource> source);
    bool setupPipeline();
    bool playPipeline();
    bool pausePipeline();
    bool seekPipeline(gint64 position);
    bool setCapsProperty(const VideoCaps &caps);
    gint64 getCurrentPosition() const;
    gint64 getDuration() const;

    /// The underlying GstPipeline, or nullptr before setupPipeline().
    /// Phase 7 (README lines 327-355) adds a tee branch through this handle.
    GstElement *pipelineElement() const;

    /// Non-blocking bus drain. Call from the application loop. Records
    /// ERROR/EOS/WARNING into m_lastError and returns false on bus ERROR.
    /// TODO(VideoPipeline.cpp): currently declared only.
    bool pollBus();

    /// Last bus error/warning text, empty when healthy.
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

    VideoPipeline(const VideoPipeline &) = delete;
    VideoPipeline &operator=(const VideoPipeline &) = delete;
    VideoPipeline(VideoPipeline &&) = delete;
    VideoPipeline &operator=(VideoPipeline &&) = delete;

    bool isPipelineSetup() const;
    template <typename... T>
    bool isPipelineInState(T... state) const;
    bool cleanup();
    bool cleanupSubElements();
};