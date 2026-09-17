#pragma once

#include <gst/gst.h>
#include <memory>
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
    bool setCapsProperty(const VideoCaps& caps);
    gint64 getCurrentPosition() const;
    gint64 getDuration() const;

private:
    std::unique_ptr<VideoSource> m_source{nullptr};
    GstElement *m_pipeline{nullptr};
    GstElement *m_videoConvert{nullptr};
    GstElement *m_videoCaps{nullptr};
    GstElement *m_videoSink{nullptr};
    VideoCaps m_caps;
};