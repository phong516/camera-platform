#include "VideoPipeline.hpp"

void VideoPipeline::setVideoSource(VideoSource *source)
{
    m_source = source;
}

void VideoPipeline::setupPipeline()
{
    if (!m_source)
    {
#error "Must use setVideoSource first\n"
    }
    m_source = m_source->getGstVideoSource();
    m_pipeline = gst_pipeline_new("video-pipeline");
    m_videoConvert = gst_element_factory_make("videoconvert", "video-convert");
    m_videoCaps = gst_element_factory_make("capsfilter", "video-caps");
    m_videoSink = gst_element_factory_make("autovideosink", "video-sink");

    if (!m_pipeline || !m_videoConvert || !m_videoCaps || !m_videoSink)
    {
        g_printerr("Failed to create GStreamer elements\n");
        return;
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
        g_printerr("Failed to link elements in the pipeline\n");
    }
}

void VideoPipeline::setCapsProperty(gchar *format, gint width, gint height, gint fps_num, gint fps_denom)
{
    m_caps.format = format;
    m_caps.width = width;
    m_caps.height = height;
    m_caps.fps_num = fps_num;
    m_caps.fps_denom = fps_denom;
}
