#pragma once

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
    void setVideoSource(VideoSource *source);
    void setupPipeline();
    void startPipeline();
    void pausePipeline();
    void stopPipeline();
    void seekPipeline(gint64 position);
    void setCapsProperty(gchar *format, gint width, gint height, gint fps_num, gint fps_denom);

private:
    VideoSource *m_source{nullptr};
    GstElement *m_pipeline{nullptr}, *m_videoConvert{nullptr}, *m_videoCaps{nullptr}, *m_videoSink{nullptr};
    VideoCaps m_caps;
};