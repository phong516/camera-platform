#include "ProfileHost.hpp"
#include "Protocol.hpp"
#include "VideoPipeline.hpp"
#include "glib.h"
#include "gst/app/gstappsink.h"
#include "gst/gstbus.h"
#include "gst/gstelement.h"
#include "gst/gstmessage.h"
#include "gst/gstobject.h"
#include <algorithm>

ProfileHost::ProfileHost(std::vector<ProfileInfo> profiles)
{
    for (const ProfileInfo &profile : profiles)
    {
        m_profileBins.push_back(std::make_unique<ProfileBin>(profile));
    }
}

ProfileHost::~ProfileHost()
{
    detachBusWatch();
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
    g_object_set(m_sourceCaps, "caps", defaultCaps, NULL);
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
    auto it =
        std::find_if(m_profileBins.begin(), m_profileBins.end(),
                     [&](const std::unique_ptr<ProfileBin> &profileBin) { return profileBin->profile().name == name; });
    if (it == m_profileBins.end())
    {
        return PipelineOutcome::failure("Profile not found");
    }
    it->get()->setDrop(!enabled);
    if (enabled)
    {
        it->get()->setState(proto::kStateReady);
    }
    else
    {
        it->get()->setState(proto::kStateOff);
    }
    return PipelineOutcome::success();
}

bool ProfileHost::profileEnabled(const std::string &name) const
{
    auto it =
        std::find_if(m_profileBins.begin(), m_profileBins.end(),
                     [&](const std::unique_ptr<ProfileBin> &profileBin) { return profileBin->profile().name == name; });
    if (it != m_profileBins.end())
    {
        return !it->get()->isDropped();
    }
    return false;
}
PipelineOutcome ProfileHost::setProfileState(const std::string &name, const std::string &state,
                                             const std::string &lastError)
{
    if (state != proto::kStateError && state != proto::kStateReady && state != proto::kStateOff &&
        state != proto::kStateLive)
    {
        return PipelineOutcome::failure("Set invalid state");
    }
    auto it =
        std::find_if(m_profileBins.begin(), m_profileBins.end(),
                     [&](const std::unique_ptr<ProfileBin> &profileBin) { return profileBin->profile().name == name; });
    if (it == m_profileBins.end())
    {
        return PipelineOutcome::failure("Profile not found");
    }
    it->get()->setState(state, lastError);
    return PipelineOutcome::success();
}
PipelineOutcome ProfileHost::attachBusWatch(GMainContext *context, PipelineEventCallback callback)
{
    if (!context || !callback)
    {
        return PipelineOutcome::failure("Invalid context or callback");
    }
    if (!m_isLinked)
    {
        return PipelineOutcome::failure("Pipeline is not setup");
    }
    detachBusWatch();
    GstBus *bus = gst_element_get_bus(m_pipeline);
    if (!bus)
    {
        return PipelineOutcome::failure("Failed to get bus from pipeline");
    }
    m_busSource = gst_bus_create_watch(bus);
    if (!m_busSource)
    {
        return PipelineOutcome::failure("Failed to create bus watch");
    }
    m_appCallback = callback;
    g_source_set_callback(m_busSource, &ProfileHost::onBusMessageStatic, this, NULL);
    g_source_attach(m_busSource, context);
    gst_object_unref(bus);
    return PipelineOutcome::success();
}

void ProfileHost::detachBusWatch()
{
    if (m_busSource)
    {
        g_source_destroy(m_busSource);
        g_source_unref(m_busSource);
        m_busSource = nullptr;
    }
}
std::vector<ProfileInfo> ProfileHost::profiles() const
{
    std::vector<ProfileInfo> profileInfos{};
    for (const auto &profileInfo : m_profileBins)
    {
        profileInfos.push_back(profileInfo->profile());
    }
    return profileInfos;
}
GstAppSink *ProfileHost::appSinkForProfile(const std::string &name) const
{
    GstAppSink *appSink{nullptr};
    auto it =
        std::find_if(m_profileBins.begin(), m_profileBins.end(),
                     [&](const std::unique_ptr<ProfileBin> &profileBin) { return profileBin->profile().name == name; });
    if (it != m_profileBins.end())
    {
        appSink = GST_APP_SINK(it->get()->appSink());
    }
    return appSink;
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
gboolean ProfileHost::onBusMessageStatic(gpointer userData)
{
    auto self = static_cast<ProfileHost *>(userData);
    if (!self->m_busSource)
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
void ProfileHost::handleBusMessage(GstMessage *msg)
{

    if (!msg || !m_appCallback)
    {
        return;
    }
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ERROR: {
        m_appCallback(PipelineEvent::Error, parseMessage(msg, &gst_message_parse_error));
        break;
    }
    case GST_MESSAGE_WARNING: {
        m_appCallback(PipelineEvent::Warning, parseMessage(msg, &gst_message_parse_warning));
        break;
    }
    case GST_MESSAGE_EOS: {
        m_appCallback(PipelineEvent::EndOfStream, "End of Stream");
        break;
    }
    default:
        break;
    }
}
std::string ProfileHost::parseMessage(GstMessage *msg, MessageParse parser)
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
