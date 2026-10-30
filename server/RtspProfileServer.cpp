#include "RtspProfileServer.hpp"
#include "ProfileHost.hpp"
#include "VideoPipeline.hpp"
#include "glib.h"
#include "gst/app/gstappsink.h"
#include "gst/gstelement.h"
#include <memory>

struct RtspProfileServer::Mount
{
	std::string name {""};
	GstAppSink *appSink {nullptr};
	GstRTSPMediaFactory *factory {nullptr};
};

RtspProfileServer::RtspProfileServer(ProfileHost &host, std::uint16_t port): m_host(host), m_port(port)
{
	m_server = gst_rtsp_server_new();
	gst_rtsp_server_set_service(m_server, std::to_string(m_port).c_str());
	GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(m_server);

	const gchar *launch = "( appsrc name=profile_src is-live=true format=time ! h264parse ! rtph264pay name=pay0 pt=96 )";
	for (const auto& profile : m_host.profiles())
	{
		GstRTSPMediaFactory *factory {nullptr};
		const std::string &name = profile.name;
		GstAppSink *appSink {nullptr};

		std::unique_ptr<Mount> mount = std::make_unique<Mount>(Mount{"/" + name, appSink, factory});

		factory = gst_rtsp_media_factory_new();
		gst_rtsp_media_factory_set_launch(factory, launch);
		gst_rtsp_media_factory_set_shared(factory, TRUE);
		g_signal_connect(factory, "media-configure", G_CALLBACK(onMediaConfigure), (gpointer)profile.name.c_str());

		gst_rtsp_mount_points_add_factory(mounts, mount->name.c_str(), mount->factory);
		gst_object_unref(mounts);

		appSink = m_host.appSinkForProfile(name);
		g_signal_connect(appSink, "new-sample", G_CALLBACK(onNewSample), NULL);

		m_mounts.push_back(std::move(mount));
	}

}
PipelineOutcome RtspProfileServer::attach(GMainContext &context)
{ 
	m_attachId = gst_rtsp_server_attach(m_server, &context);
	if (!m_attachId)
	{
		return PipelineOutcome::failure("Failed to attach server to context");
	}
	return PipelineOutcome::success();
}
void RtspProfileServer::detach()
{
	
	g_source_remove(m_attachId);
}
void RtspProfileServer::onMediaConfigure(GstRTSPMediaFactory *factory,
                                         GstRTSPMedia *media,
                                         gpointer userData)
{
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
