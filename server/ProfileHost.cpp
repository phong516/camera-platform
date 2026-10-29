#include "ProfileHost.hpp"
#include <algorithm>

ProfileHost::ProfileHost(std::vector<ProfileInfo> profiles)
{
    for (const ProfileInfo& profile : profiles)
    {
        m_profileBins.push_back(std::make_unique<ProfileBin>(profile));
    }
}

ProfileHost::~ProfileHost()
{
    cleanup();
}

PipelineOutcome ProfileHost::setVideoSource(std::unique_ptr<VideoSource> source)
{
    m_videoSource.reset(source.release());
    return PipelineOutcome::success();
}

PipelineOutcome ProfileHost::setupPipeline()
{
    if (!m_videoSource)
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
        cleanup();
        return PipelineOutcome::failure("Failed to create GStreamer elements");
    }
    
    GstCaps *defaultCaps = VideoCaps().toGstCaps();
    g_object_set(m_sourceCaps,
                 "caps", defaultCaps,
                 NULL);
    gst_caps_unref(defaultCaps);

    gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoConvert, m_sourceCaps, m_tee, NULL);
    if (!gst_element_link_many(m_source, m_videoConvert, m_sourceCaps, m_tee, NULL))
    {
        return PipelineOutcome::failure("Failed to link GStreamer elements");
    }

    m_isLinked = true;

    for (auto &profileBin : m_profileBins)
    {
        GstElement *bin = profileBin->createElement();
        if (!bin)
        {
            cleanup();
            return PipelineOutcome::failure("Failed to create profile bin element");
        }
        gst_bin_add(GST_BIN(m_pipeline), bin);
        GstPad *teePad = gst_element_request_pad_simple(m_tee, "src_%u");
        GstPad *binPad = gst_element_get_static_pad(bin, "sink");

        if (gst_pad_link(teePad, binPad) != GST_PAD_LINK_OK)
        {
            gst_object_unref(binPad);
            gst_object_unref(teePad);
            cleanup();
            return PipelineOutcome::failure("Failed to link tee pad to profile bin sink pad");
        }
        gst_object_unref(binPad);
        gst_object_unref(teePad);
    }

    return PipelineOutcome::success();
}

PipelineOutcome ProfileHost::play()
{
    if (!m_isLinked)
    {
        return PipelineOutcome::failure("Pipeline not setup");
    }
    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        return PipelineOutcome::failure("Failed to set pipeline to PLAYING");
    }
    return PipelineOutcome::success();
}

PipelineOutcome ProfileHost::stop()
{
    if (!m_isLinked)
    {
        return PipelineOutcome::failure("Pipeline not setup");
    }
    if (gst_element_set_state(m_pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    {
        return PipelineOutcome::failure("Failed to set pipeline to PAUSED");
    }
    return PipelineOutcome::success();
}

PipelineOutcome ProfileHost::setProfileEnabled(const std::string &name, bool enabled)
{
    auto it = std::find_if(m_profileBins.begin(), m_profileBins.end(), [&](const std::unique_ptr<ProfileBin> &profileBin)
    {
        return profileBin->profile().name == name;
    });
    if (it == m_profileBins.end())
    {
        return PipelineOutcome::failure("Profile not found");
    }
    it->get()->setDrop(!enabled);
    return PipelineOutcome::success();
}

bool ProfileHost::profileEnabled(const std::string &name) const
{
    auto it = std::find_if(m_profileBins.begin(), m_profileBins.end(), [&](const std::unique_ptr<ProfileBin> &profileBin)
    {
        return profileBin->profile().name == name;
    });
    if (it != m_profileBins.end())
    {
        return !it->get()->isDropped();
    }
    return false;
}
std::vector<ProfileInfo> ProfileHost::profiles() const { return {}; }
GstAppSink *ProfileHost::appSinkForProfile(const std::string &name) const {
  return {};
}

void ProfileHost::cleanup()
{
    if (m_isLinked)
    {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_clear_object(&m_pipeline);
        m_source = nullptr;
        m_videoConvert = nullptr;
        m_sourceCaps = nullptr;
        m_tee = nullptr;
    }
    else
    {
        gst_clear_object(&m_pipeline);
        gst_clear_object(&m_source);
        gst_clear_object(&m_videoConvert);
        gst_clear_object(&m_sourceCaps);
        gst_clear_object(&m_tee);
    }
    m_isLinked = false;
}
