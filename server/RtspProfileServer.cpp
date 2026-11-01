#include "RtspProfileServer.hpp"
#include "ProfileHost.hpp"
#include "VideoPipeline.hpp"
#include "glib.h"
#include "gst/app/gstappsink.h"
#include "gst/app/gstappsrc.h"
#include "gst/gstbin.h"
#include "gst/gstbuffer.h"
#include "gst/gstclock.h"
#include "gst/gstpad.h"
#include "gst/gstsample.h"
#include <memory>

struct RtspProfileServer::Mount
{
	std::string name {""};
	GstAppSink *appSink {nullptr};
	GstAppSrc *appSrc {nullptr};
	GstRTSPMediaFactory *factory {nullptr};
};

RtspProfileServer::RtspProfileServer(ProfileHost &host, std::uint16_t port): m_host(host), m_port(port)
{
	m_server = gst_rtsp_server_new();
	g_object_ref_sink(m_server);

	gst_rtsp_server_set_service(m_server, std::to_string(m_port).c_str());
	GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(m_server);

	const gchar *launch = "( appsrc name=profile_src is-live=true format=time do-timestamp=true ! h264parse ! rtph264pay name=pay0 pt=96 config-interval=1 )";
	for (const auto& profile : m_host.profiles())
	{
		auto mount = std::make_unique<Mount>();
		mount->name = profile.name;

		mount->factory = gst_rtsp_media_factory_new();
		gst_rtsp_media_factory_set_launch(mount->factory, launch);
		gst_rtsp_media_factory_set_shared(mount->factory, TRUE);

		g_signal_connect(mount->factory, "media-configure", G_CALLBACK(onMediaConfigure), (gpointer)&mount);

		mount->appSink = m_host.appSinkForProfile(profile.name);
		if (mount->appSink)
		{
			g_signal_connect(mount->appSink, "new-sample", G_CALLBACK(onNewSample), (gpointer)&mount);
		}

		gst_rtsp_mount_points_add_factory(mounts, ("/" + profile.name).c_str(), mount->factory);
		m_mounts.push_back(std::move(mount));
	}
	g_object_unref(mounts);
}
RtspProfileServer::~RtspProfileServer()
{
	detach();
	g_object_unref(m_server);
}
PipelineOutcome RtspProfileServer::attach(GMainContext &context)
{ 
	if (!m_server)
	{
		return PipelineOutcome::failure("No server available");
	}
	if (m_source)
	{
		return PipelineOutcome::failure("Server already attached");
	}
	GError *error{nullptr};
	m_source = gst_rtsp_server_create_source(m_server, NULL, &error);
	if (m_source == nullptr)
	{
		return PipelineOutcome::failure("Failed to attach rtsp server: " + std::string(error->message));
	}
	g_source_attach(m_source, &context);
	return PipelineOutcome::success();
}
void RtspProfileServer::detach()
{
	if (!m_source)
	{
		return;
	}
	g_source_destroy(m_source);
	g_source_unref(m_source);
	m_source = nullptr;
}
void RtspProfileServer::onMediaConfigure(GstRTSPMediaFactory *factory,
                                         GstRTSPMedia *media,
                                         gpointer userData)
{
	Mount *mount = (Mount *)userData;
	mount->appSrc = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(media), "profile_src"));

	g_signal_connect(media, "prepared", G_CALLBACK(onMediaPrepared), userData);

	g_signal_connect(media, "unprepared", G_CALLBACK(onMediaUnprepared), userData);
}
void RtspProfileServer::onMediaPrepared(GstRTSPMedia *media,
                                        gpointer userData) 
{
	g_print("[server] Profile %s started serving\n", (const gchar *)userData);
}
void RtspProfileServer::onMediaUnprepared(GstRTSPMedia *media,
                                          gpointer userData)
{
	g_print("[server] Profile %s stopped serving\n", (const gchar *)userData);
}
GstFlowReturn RtspProfileServer::onNewSample(GstAppSink *sink,
                                             gpointer userData) 
{
	Mount *mount = (Mount *)userData;
	GstSample *sample = gst_app_sink_pull_sample(sink);
	if (sample == NULL)
	{
		return GST_FLOW_ERROR;
	}
	GstBuffer *buffer = gst_buffer_copy(gst_sample_get_buffer(sample));
	GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
	GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
	GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

	if (mount->appSrc != NULL)
	{
		gst_app_src_set_caps(mount->appSrc, gst_sample_get_caps(sample));
		gst_app_src_push_buffer(mount->appSrc, buffer);
	}
	else
	{
		gst_buffer_unref(buffer);
	}
	gst_sample_unref(sample);
	return GST_FLOW_OK;
}
