#include "VideoPipeline.hpp"
#include <initializer_list>
#include <algorithm>

VideoPipeline::VideoPipeline(): m_videoSink(std::make_unique<AutoVideoSink>())
{
}

VideoPipeline::~VideoPipeline()
{
    stop();
}

PipelineOutcome VideoPipeline::setVideoSource(std::unique_ptr<VideoSource> source)
{
    m_videoSource.reset();
    m_videoSource = std::move(source);
    return PipelineOutcome::success();
}

PipelineOutcome VideoPipeline::setVideoSink(std::unique_ptr<VideoSink> sink)
{
    m_videoSink.reset();
    m_videoSink = std::move(sink);
    return PipelineOutcome::success();
}

PipelineOutcome VideoPipeline::setupPipeline()
{
    if (isPipelineSetup())
    {
        g_critical("Pipeline is already set up. Please reset the pipeline before setting up a new one.");
        return PipelineOutcome::failure("Pipeline is setup");
    }
    if (!m_videoSource)
    {
        g_critical("Video source is not set. Please set a video source before setting up the pipeline.");
        return PipelineOutcome::failure("videoSource is not set");
    }
    m_source = m_videoSource->createElement();
    m_pipeline = gst_pipeline_new("video-pipeline");
    m_videoConvert = gst_element_factory_make("videoconvert", "video-convert");
    m_videoCaps = gst_element_factory_make("capsfilter", "video-caps");
    m_sink = gst_element_factory_make("autovideosink", "video-sink");

    if (!isPipelineSetup())
    {
        g_critical("Failed to create GStreamer elements\n");
        stop();
        return PipelineOutcome::failure("Failed to create GStreamer elements");
    }

    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, m_caps.format,
                                        "width", G_TYPE_INT, m_caps.width,
                                        "height", G_TYPE_INT, m_caps.height,
                                        "framerate", GST_TYPE_FRACTION, m_caps.fps_num, m_caps.fps_denom,
                                        NULL);

    g_object_set(m_videoCaps, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoConvert, m_videoCaps, m_sink, NULL);

    if (!gst_element_link_many(m_source, m_videoConvert, m_videoCaps, m_sink, NULL))
    {
        g_critical("Failed to link elements in the pipeline\n");
        stop();
        return PipelineOutcome::failure("Failed to link elements in the pipeline");
    }

    return PipelineOutcome::success();
}

PipelineOutcome VideoPipeline::play()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return PipelineOutcome::failure("Pipeline is not setup");
    }

    if (!isPipelineInState(GST_STATE_NULL, GST_STATE_READY, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in a valid state to play\n");
        return PipelineOutcome::failure("Invalid pipeline state for play");
    }
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_critical("Failed to set pipeline to PLAYING state\n");
        return PipelineOutcome::failure("Failed to set pipeline to PLAYING state");
    }
    return PipelineOutcome::success();
}

PipelineOutcome VideoPipeline::pause()
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return PipelineOutcome::failure("Pipeline is not setup");
    }
    if (!isPipelineInState(GST_STATE_PLAYING))
    {
        g_critical("Pipeline is not in PLAYING state, cannot pause\n");
        return PipelineOutcome::failure("Pipeline is not in PLAYING state, cannot pause");
    }
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_critical("Failed to set pipeline to PAUSED state\n");
        return PipelineOutcome::failure("Failed to set pipeline to PAUSED state");
    }
    return PipelineOutcome::success();
}

PipelineOutcome VideoPipeline::stop()
{
    detachBusWatch();
    if (m_pipeline)
    {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_element_get_state(m_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
        gst_clear_object(&m_pipeline);
        m_source = nullptr;
        m_videoConvert = nullptr;
        m_videoCaps = nullptr;
        m_sink = nullptr;
    }
    gst_clear_object(&m_source);
    gst_clear_object(&m_videoConvert);
    gst_clear_object(&m_videoCaps);
    gst_clear_object(&m_sink);
    return PipelineOutcome::success();
}

PipelineOutcome VideoPipeline::seek(gint64 position)
{
    if (!isPipelineSetup())
    {
        g_critical("Please setup pipeline first\n");
        return PipelineOutcome::failure("Pipeline is not setup");
    }

    if (!isPipelineInState(GST_STATE_PLAYING, GST_STATE_PAUSED))
    {
        g_critical("Pipeline is not in PLAYING or PAUSED state, cannot seek\n");
        return PipelineOutcome::failure("Pipeline is not in PLAYING or PAUSED state, cannot seek");
    }

    return gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), position) ? PipelineOutcome::success() : PipelineOutcome::failure("Pipeline cannot be seeked");
}

PipelineOutcome VideoPipeline::setCaps(const VideoCaps &caps)
{
    m_caps = caps;
    if (!isPipelineSetup())
        return PipelineOutcome::success();

    // Apply the caps to the videoCaps element if the pipeline is set up
    GstCaps *gstCaps = caps.toGstCaps();
    g_object_set(m_videoCaps, "caps", gstCaps, nullptr);
    gst_caps_unref(gstCaps);
    return PipelineOutcome::success();
}

gint64 VideoPipeline::currentPosition() const
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

gint64 VideoPipeline::duration() const
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

PipelineOutcome VideoPipeline::attachBusWatch(GMainContext *context, PipelineEventCallback callback)
{
    if (!context || !callback)
    {
        return PipelineOutcome::failure("Invalid context or callback");
    }
    if (!isPipelineSetup())
    {
        return PipelineOutcome::failure("Pipeline is not setup");
    }
    detachBusWatch();
    GstBus *bus = gst_element_get_bus(m_pipeline);
    if (!bus)
        return PipelineOutcome::failure("Failed to get bus from pipeline");
    m_busSource = gst_bus_create_watch(bus);
    if (!m_busSource)
    {
        return PipelineOutcome::failure("Failed to create bus watch");
    }
    m_appCallback = callback;
    g_source_set_callback(m_busSource, &VideoPipeline::onBusMessageStatic, this, NULL);
    g_source_attach(m_busSource, context);
    gst_object_unref(bus);
    return PipelineOutcome::success();
}

void VideoPipeline::detachBusWatch()
{
    if (m_busSource)
    {
        g_source_destroy(m_busSource);
        g_source_unref(m_busSource);
        m_busSource = nullptr;
    }
}

bool VideoPipeline::isPipelineSetup() const
{
    return m_pipeline && m_source && m_videoConvert && m_videoCaps && m_sink;
}

gboolean VideoPipeline::onBusMessageStatic(gpointer userData)
{
    auto self = static_cast<VideoPipeline *>(userData);
    if (!self->isPipelineSetup())
    {
        return G_SOURCE_CONTINUE;
    }
    GstBus *bus = gst_element_get_bus(self->m_pipeline);
    GstMessage *msg = gst_bus_pop(bus);
    gst_object_unref(bus);
    if (msg)
    {
        self->handleBusMessage(msg);
    }
    gst_message_unref(msg);
    return G_SOURCE_CONTINUE;
}

void VideoPipeline::handleBusMessage(GstMessage *msg)
{
    if (!msg || !m_appCallback)
    {
        return;
    }
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ERROR:
    {
        m_appCallback(PipelineEvent::Error, parseMessage(msg, &gst_message_parse_error));
        break;
    }
    case GST_MESSAGE_WARNING:
    {
        m_appCallback(PipelineEvent::Warning, parseMessage(msg, &gst_message_parse_warning));
        break;
    }
    case GST_MESSAGE_EOS:
    {
        m_appCallback(PipelineEvent::EndOfStream, "End of Stream");
        break;
    }
    }
}

std::string VideoPipeline::parseMessage(GstMessage *msg, MessageParse parser)
{
    GError *error{nullptr};
    gchar *debug{nullptr};
    parser(msg, &error, &debug);
    std::string message = error ? error->message : "unknown";
    if (debug)
    {
        message += ": ";
        message += debug;
    }
    g_clear_error(&error);
    g_free(debug);
    return message;
}

template <typename... T>
bool VideoPipeline::isPipelineInState(T... states) const
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
    GstState stateArray[] = {states...};
    return std::any_of(std::begin(stateArray), std::end(stateArray), [currentState](GstState state)
                       { return state == currentState; });
}

GstCaps *VideoCaps::toGstCaps() const
{
    GstCaps *caps = gst_caps_new_simple("video/x-raw", 
                                        "format", G_TYPE_STRING, format, 
                                        "width", G_TYPE_INT, width,
                                        "height", G_TYPE_INT, height, 
                                        "framerate", GST_TYPE_FRACTION, fps_num, fps_denom, NULL);
    return caps ? caps : nullptr;
}
