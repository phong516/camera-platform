#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

static void media_prepared(GstRTSPMedia *media, gpointer user_data);
static void media_unprepared(GstRTSPMedia *media, gpointer user_data);

static void media_configured(GstRTSPMediaFactory *factory, GstRTSPMedia *media,
                             gpointer user_data) {
  const gchar *path = (const gchar *)user_data;
  g_print("[server] media-configure for %s\n", path);
  g_signal_connect(media, "prepared", G_CALLBACK(media_prepared), user_data);
  g_signal_connect(media, "unprepared", G_CALLBACK(media_unprepared),
                   user_data);
  GstElement *element = gst_rtsp_media_get_element(media);
  g_print("[server]   pipeline: %s\n", GST_ELEMENT_NAME(element));
  gst_object_unref(element);
}

static void media_prepared(GstRTSPMedia *media, gpointer user_data) {
  g_print("[server] media prepared for %s\n", (const gchar *)user_data);
}

static void media_unprepared(GstRTSPMedia *media, gpointer user_data) {
  g_print("[server] media unprepared for %s\n", (const gchar *)user_data);
}

static void client_closed(GstRTSPClient *client, gpointer user_data) {
  g_print("[server] client closed\n");
}

static void client_connected(GstRTSPServer *server, GstRTSPClient *client,
                             gpointer user_data) {
  GstRTSPConnection *conn = gst_rtsp_client_get_connection(client);
  const GstRTSPUrl *url = conn ? gst_rtsp_connection_get_url(conn) : NULL;
  g_print("[server] client connected from %s:%d\n",
          url ? url->host : "?", url ? url->port : 0);
  g_signal_connect(client, "closed", G_CALLBACK(client_closed), NULL);
}

static GstRTSPMediaFactory *make_factory(const char *path, const char *launch) {
  GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
  gst_rtsp_media_factory_set_launch(factory, launch);
  gst_rtsp_media_factory_set_shared(factory, TRUE);
  g_signal_connect(factory, "media-configure", G_CALLBACK(media_configured),
                   (gpointer)path);
  return factory;
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);

  GstRTSPServer *server = gst_rtsp_server_new();
  gst_rtsp_server_set_service(server, "8554");
  g_signal_connect(server, "client-connected", G_CALLBACK(client_connected),
                   NULL);

  GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

  GstRTSPMediaFactory *hd = make_factory(
      "/hd",
      "( videotestsrc is-live=true pattern=smpte ! videoscale ! "
      "video/x-raw,width=1280,height=720,framerate=30/1 ! "
      "x264enc speed-preset=ultrafast tune=zerolatency key-int-max=30 "
      "bitrate=2500 ! rtph264pay name=pay0 pt=96 config-interval=1 )");
  gst_rtsp_mount_points_add_factory(mounts, "/hd", hd);

  GstRTSPMediaFactory *sd = make_factory(
      "/sd",
      "( videotestsrc is-live=true pattern=smpte ! videoscale ! "
      "video/x-raw,width=854,height=480,framerate=30/1 ! "
      "x264enc speed-preset=ultrafast tune=zerolatency key-int-max=30 "
      "bitrate=1200 ! rtph264pay name=pay0 pt=96 config-interval=1 )");
  gst_rtsp_mount_points_add_factory(mounts, "/sd", sd);

  gst_object_unref(mounts);

  GMainContext *context = g_main_context_new();
  GMainLoop *loop = g_main_loop_new(context, FALSE);
  if (!gst_rtsp_server_attach(server, context)) {
    g_printerr("[server] attach failed (port busy?)\n");
    return 1;
  }
  g_print("[server] ready: rtsp://127.0.0.1:8554/hd and /sd\n");
  g_main_loop_run(loop);

  g_main_loop_unref(loop);
  g_main_context_unref(context);
  gst_object_unref(server);
  return 0;
}
