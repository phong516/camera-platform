#include "VideoPipeline.hpp"
#include <initializer_list>

VideoPipeline::~VideoPipeline()
{
    if (!m_pipeline)
    {
        return;
    }
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_critical("Failed to set pipeline to NULL state\n");
    }
    gst_clear_object(&m_videoSink);
    gst_clear_object(&m_videoCaps);
    gst_clear_object(&m_videoConvert);
    GstElement *sourceElement = m_source->getGstVideoSource();
    gst_clear_object(&sourceElement);
    gst_clear_object(&m_pipeline);   
}

bool VideoPipeline::setVideoSource(std::unique_ptr<VideoSource> source)
{
    m_source = std::move(source);
    return true;
}

bool VideoPipeline::setupPipeline()
{
    if (!m_source)
    {
        g_critical("Video source is not set. Please set a video source before setting up the pipeline.");
        return false;
    }
    GstElement *sourceElement = m_source->getGstVideoSource();
    m_pipeline = gst_pipeline_new("video-pipeline");
    m_videoConvert = gst_element_factory_make("videoconvert", "video-convert");
    m_videoCaps = gst_element_factory_make("capsfilter", "video-caps");
    m_videoSink = gst_element_factory_make("autovideosink", "video-sink");

    if (!isPipelineSetup())
    {
        g_critical("Failed to create GStreamer elements\n");
        return false;
    }

    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, m_caps.format,
                                        "width", G_TYPE_INT, m_caps.width,
                                        "height", G_TYPE_INT, m_caps.height,
                                        "framerate", GST_TYPE_FRACTION, m_caps.fps_num, m_caps.fps_denom,
                                        NULL);

    g_object_set(m_videoCaps, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(m_pipeline), sourceElement, m_videoConvert, m_videoCaps, m_videoSink, NULL);

    if (!gst_element_link_many(sourceElement, m_videoConvert, m_videoCaps, m_videoSink, NULL))
    {
        g_critical("Failed to link elements in the pipeline\n");
        return false;
    }

    return true;
}

bool VideoPipeline::playPipeline()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n") ;
        return false;
    }

    GstState state{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE) 
    {
        g_warning("Failed to get state\n");
        return false;
    }
    if (state == GST_STATE_NULL || state == GST_STATE_READY || state == GST_STATE_PAUSED)
    {
        ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE)
        {
            g_critical("Failed to set pipeline to PLAYING state\n");
            return false;
        }
    }
    return true;
}

bool VideoPipeline::pausePipeline()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n") ;
        return false;
    }

    GstState state{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE) 
    {
        g_warning("Failed to get state\n");
        return false;
    }
    if (state == GST_STATE_PLAYING)
    {
        ret = gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        if (ret == GST_STATE_CHANGE_FAILURE)
        {
            g_critical("Failed to set pipeline to PAUSED state\n");
            return false;
        }
    }
    else
    {
        g_warning("Pipeline is not in PLAYING state, cannot pause\n");
        return false;
    }
    return true;
}

bool VideoPipeline::seekPipeline(gint64 position)
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n") ;
        return false;
    }

    GstState state{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE) 
    {
        g_warning("Failed to get state\n");
        return false;
    }
    if (state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
    {
        g_warning("Pipeline is not in PLAYING or PAUSED state, cannot seek\n");
        return false;
    }

    return false;
}

bool VideoPipeline::setCapsProperty(const VideoCaps& caps)
{
    m_caps = caps;
    return true;
}

gint64 VideoPipeline::getCurrentPosition() const
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n") ;
        return -1;
    }
    GstState state{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_warning("Failed to get state\n");
        return -1;
    }
    if (state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
    {
        g_warning("Pipeline is not in PLAYING or PAUSED state, cannot query duration\n");
        return -1;
    }
    gint64 duration = gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &duration) ? duration : -1;
    return duration;
}

gint64 VideoPipeline::getDuration() const
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n") ;
        return -1;
    }
    GstState state{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_warning("Failed to get state\n");
        return -1;
    }
    if (state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
    {
        g_warning("Pipeline is not in PLAYING or PAUSED state, cannot query duration\n");
        return -1;
    }
    gint64 duration = gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &duration) ? duration : -1;
    return duration;
}

bool VideoPipeline::isPipelineSetup() const
{
    return m_pipeline && m_source && m_videoConvert && m_videoCaps && m_videoSink;
}


bool VideoPipeline::isPipelineInState(GstState state, std::initializer_list<GstState> states) const
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n") ;
        return false;
    }
    GstState currentState{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &currentState, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_warning("Failed to get state\n");
        return false;
    }
    return currentState == state;
}
