#include "ProfileHost.hpp"

ProfileHost::ProfileHost(std::vector<ProfileInfo> profiles): m_profiles(profiles)
{
}

ProfileHost::~ProfileHost()
{
    if (m_pipeline)
    {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_source = nullptr;
        m_videoConvert = nullptr;
        m_sourceCaps = nullptr;
        m_tee = nullptr;
    }
    if (m_source)
    {
        gst_object_unref(m_source);
    }
    if (m_videoConvert)
    {
        gst_object_unref(m_videoConvert);
    }
    if (m_sourceCaps)
    {
        gst_object_unref(m_sourceCaps);
    }
    if (m_tee)
    {
        gst_object_unref(m_tee);
    }
}

PipelineOutcome ProfileHost::setVideoSource(std::unique_ptr<VideoSource> source)
{
    m_videoSource.reset(source.release());
    return PipelineOutcome::success();
}

PipelineOutcome ProfileHost::setupPipeline()
{
    if (!m_source)
    {
        return PipelineOutcome::failure("Video source not set");
    }
    if (m_pipeline)
    {
        return PipelineOutcome::failure("Pipeline already set up");
    }

    m_pipeline = gst_pipeline_new("pipeline");
    m_source = m_videoSource->createElement();
    m_videoConvert = gst_element_factory_make("videoconvert", "video-convert");
    m_sourceCaps = gst_element_factory_make("capsfilter", "source-caps");
    m_tee = gst_element_factory_make("tee", "tee");

    if (!m_pipeline || !m_source || !m_videoConvert || !m_sourceCaps || !m_tee)
    {
        g_clear_object(&m_pipeline);
        g_clear_object(&m_source);
        g_clear_object(&m_videoConvert);
        g_clear_object(&m_sourceCaps);
        g_clear_object(&m_tee);
        return PipelineOutcome::failure("Failed to create GStreamer elements");
    }
    return PipelineOutcome::success();
}
