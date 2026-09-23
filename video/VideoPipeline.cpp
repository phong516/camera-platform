#include "VideoPipeline.hpp"
#include <initializer_list>
#include <algorithm>

VideoPipeline::~VideoPipeline()
{
    cleanup();
}

bool VideoPipeline::setVideoSource(std::unique_ptr<VideoSource> source)
{
    if (isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return false;
    }
    m_videoSource.reset();
    m_videoSource = std::move(source);
    return true;
}

bool VideoPipeline::setupPipeline()
{
    if (isPipelineSetup())
    {
        g_critical("Pipeline is already set up. Please reset the pipeline before setting up a new one.\n");
        return false;
    }
    if (!m_videoSource)
    {
        g_critical("Video source is not set. Please set a video source before setting up the pipeline.");
        return false;
    }
    m_source = m_videoSource->createElement();
    m_pipeline = gst_pipeline_new("video-pipeline");
    m_videoConvert = gst_element_factory_make("videoconvert", "video-convert");
    m_videoCaps = gst_element_factory_make("capsfilter", "video-caps");
    m_videoSink = gst_element_factory_make("autovideosink", "video-sink");

    if (!isPipelineSetup())
    {
        g_critical("Failed to create GStreamer elements\n");
        cleanup();
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

    gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoConvert, m_videoCaps, m_videoSink, NULL);

    if (!gst_element_link_many(m_source, m_videoConvert, m_videoCaps, m_videoSink, NULL))
    {
        g_critical("Failed to link elements in the pipeline\n");
        cleanup();
        return false;
    }

    return true;
}

bool VideoPipeline::playPipeline()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return false;
    }

    if (!isPipelineInState(GST_STATE_NULL, GST_STATE_READY, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in a valid state to play\n");
        return false;
    }
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_critical("Failed to set pipeline to PLAYING state\n");
        return false;
    }
    return true;
}

bool VideoPipeline::pausePipeline()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return false;
    }
    if (!isPipelineInState(GST_STATE_PLAYING))
    {
        g_critical("Pipeline is not in PLAYING state, cannot pause\n");
        return false;
    }
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_critical("Failed to set pipeline to PAUSED state\n");
        return false;
    }
    return true;
}

bool VideoPipeline::stopPipeline()
{
    cleanup();
    return true;
}

bool VideoPipeline::seekPipeline(gint64 position)
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return false;
    }

    if (!isPipelineInState(GST_STATE_PLAYING, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in PLAYING or PAUSED state, cannot seek\n");
        return false;
    }

    return gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), position);
}

bool VideoPipeline::setCapsProperty(const VideoCaps &caps)
{
    m_caps = caps;
    if (!isPipelineSetup())
        return true;
    
    // Apply the caps to the videoCaps element if the pipeline is set up
    g_object_set(m_videoCaps, "caps", m_caps.toGstCaps(), nullptr);
    return true;
}

gint64 VideoPipeline::getCurrentPosition() const
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return -1;
    }
    if (!isPipelineInState(GST_STATE_PLAYING, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in PLAYING or PAUSED state, cannot get current position\n");
        return -1;
    }
    gint64 duration = -1;
    if (!gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &duration))
    {
        g_critical("Failed to query current position\n");
        return -1;
    }
    return duration;
}

gint64 VideoPipeline::getDuration() const
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return -1;
    }
    if (!isPipelineInState(GST_STATE_PLAYING, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in PLAYING or PAUSED state, cannot get duration\n");
        return -1;
    }
    gint64 duration = gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &duration) ? duration : -1;
    return duration;
}

GstElement *VideoPipeline::pipelineElement() const
{
    if (isPipelineSetup())
    {
        return m_pipeline;
    }
    return nullptr;
}

bool VideoPipeline::pollBus()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return false;
    }
    if (!isPipelineInState(GST_STATE_PLAYING, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in PLAYING or PAUSED state, cannot poll bus\n");
        return false;
    }
    GstBus *bus = gst_element_get_bus(m_pipeline);
    GstMessage *msg = gst_bus_pop_filtered(bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR| GST_MESSAGE_EOS));
    gst_object_unref(bus);
    if (!msg)
    {
        m_lastError.clear();
        return true;
    }
    m_lastError = gst_message_type_get_name(GST_MESSAGE_TYPE(msg));
    gst_message_unref(msg);
    return true;
}

std::string VideoPipeline::lastError() const
{
    return m_lastError;
}

bool VideoPipeline::isPipelineSetup() const
{
    return m_pipeline && m_source && m_videoConvert && m_videoCaps && m_videoSink;
}

bool VideoPipeline::cleanup()
{
    if (m_pipeline)
    {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_clear_object(&m_pipeline);
        m_source = nullptr;
        m_videoConvert = nullptr;
        m_videoCaps = nullptr;
        m_videoSink = nullptr;
    }
    else
    {
        cleanupSubElements();
    }
    return true;
}

bool VideoPipeline::cleanupSubElements()
{
    if (m_source)
    {
        gst_clear_object(&m_source);
    }
    if (m_videoConvert)
    {
        gst_clear_object(&m_videoConvert);
    }
    if (m_videoCaps)
    {
        gst_clear_object(&m_videoCaps);
    }
    if (m_videoSink)
    {
        gst_clear_object(&m_videoSink);
    }

    return true;
}

template <typename... T>
bool VideoPipeline::isPipelineInState(T... state) const
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return false;
    }
    GstState currentState{}, pending{};
    GstStateChangeReturn ret = gst_element_get_state(m_pipeline, &currentState, &pending, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_warning("Failed to get state\n");
        return false;
    }
    GstState stateArray[] = {state...};
    return std::any_of(std::begin(stateArray), std::end(stateArray), [currentState](GstState state)
                       { return state == currentState; });
}
