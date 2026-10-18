#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtsp-server/rtsp-server.h>

typedef struct {
  const gchar *mount;
  GstElement *valve;
  GstElement *appsrc;
  guint session;
  guint pushed;
  guint dropped;
  guint pushfail;
  gboolean caps_set;
  GstCaps *last_caps;
  GstBuffer *last_buf;
} Profile;

static GstElement *pipeline;
static Profile profiles[2];
static GMutex lock;
static GMainLoop *loop;

static gint64 now_ms(void) { return g_get_monotonic_time() / 1000; }

static void on_media_prepared(GstRTSPMedia *media, gpointer user_data) {
  Profile *p = (Profile *)user_data;
  g_print("[t=%lldms] [%s] media prepared\n", (long long)now_ms(), p->mount);
}

static void on_media_state(GstRTSPMedia *media, gint state, gpointer user_data) {
  Profile *p = (Profile *)user_data;
  g_print("[t=%lldms] [%s] media new-state: %d\n", (long long)now_ms(),
          p->mount, state);
}

static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
  Profile *p = (Profile *)user_data;
  GstSample *sample = gst_app_sink_pull_sample(sink);
  if (sample == NULL) {
    return GST_FLOW_ERROR;
  }
  GstBuffer *buffer = gst_buffer_copy(gst_sample_get_buffer(sample));
  GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

  g_mutex_lock(&lock);
  gst_caps_replace(&p->last_caps, gst_sample_get_caps(sample));
  gst_buffer_replace(&p->last_buf, buffer);
  if (p->appsrc != NULL) {
    if (!p->caps_set) {
      gst_app_src_set_caps(GST_APP_SRC(p->appsrc), gst_sample_get_caps(sample));
      p->caps_set = TRUE;
    }
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(p->appsrc), buffer);
    if (ret == GST_FLOW_OK) {
      p->pushed++;
    } else {
      p->pushfail++;
    }
  } else {
    gst_buffer_unref(buffer);
    p->dropped++;
  }
  g_mutex_unlock(&lock);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

static void on_media_unprepared(GstRTSPMedia *media, gpointer user_data) {
  Profile *p = (Profile *)user_data;
  g_mutex_lock(&lock);
  guint pushed = p->pushed;
  if (p->appsrc != NULL) {
    gst_object_unref(p->appsrc);
    p->appsrc = NULL;
  }
  g_mutex_unlock(&lock);
  g_object_set(p->valve, "drop", TRUE, NULL);
  g_print("[t=%lldms] [%s] viewers gone -> valve CLOSED (pushed %u)\n",
          (long long)now_ms(), p->mount, pushed);
}

static void on_media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media,
                               gpointer user_data) {
  Profile *p = (Profile *)user_data;
  GstElement *element = gst_rtsp_media_get_element(media);
  GstElement *appsrc = gst_bin_get_by_name(GST_BIN(element), "vsrc");
  gst_object_unref(element);
  g_mutex_lock(&lock);
  if (p->appsrc != NULL) {
    gst_object_unref(p->appsrc);
  }
  p->appsrc = appsrc;
  p->session++;
  p->pushed = 0;
  p->caps_set = FALSE;
  guint session = p->session;
  if (p->last_caps != NULL && p->last_buf != NULL) {
    gst_app_src_set_caps(GST_APP_SRC(appsrc), p->last_caps);
    p->caps_set = TRUE;
    gst_app_src_push_buffer(GST_APP_SRC(appsrc), gst_buffer_copy(p->last_buf));
    g_print("[t=%lldms] [%s] pre-seeded appsrc with latest frame\n",
            (long long)now_ms(), p->mount);
  }
  g_mutex_unlock(&lock);
  g_signal_connect(media, "unprepared", G_CALLBACK(on_media_unprepared), p);
  g_signal_connect(media, "prepared", G_CALLBACK(on_media_prepared), p);
  g_signal_connect(media, "new-state", G_CALLBACK(on_media_state), p);
  g_object_set(p->valve, "drop", FALSE, NULL);
  g_print("[t=%lldms] [%s] viewer arrived -> session %u: valve OPEN\n",
          (long long)now_ms(), p->mount, session);
}

static gboolean on_status(gpointer user_data) {
  for (int i = 0; i < 2; i++) {
    Profile *p = &profiles[i];
    g_mutex_lock(&lock);
    guint session = p->session;
    guint pushed = p->pushed;
    guint dropped = p->dropped;
    guint pushfail = p->pushfail;
    gboolean wired = p->appsrc != NULL;
    g_mutex_unlock(&lock);
    gboolean drop = FALSE;
    g_object_get(p->valve, "drop", &drop, NULL);
    g_print("[status] %s session=%u pushed=%u dropped=%u pushfail=%u "
            "valve_drop=%s wired=%s\n",
            p->mount, session, pushed, dropped, pushfail,
            drop ? "yes" : "no", wired ? "yes" : "no");
  }
  return G_SOURCE_CONTINUE;
}

static gboolean on_quit(gpointer user_data) {
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

static GstRTSPMediaFactory *make_factory(Profile *p) {
  GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
  gst_rtsp_media_factory_set_launch(
      factory,
      "( appsrc name=vsrc is-live=true do-timestamp=true ! h264parse ! "
      "rtph264pay name=pay0 pt=96 config-interval=1 )");
  gst_rtsp_media_factory_set_shared(factory, TRUE);
  g_signal_connect(factory, "media-configure", G_CALLBACK(on_media_configure),
                   p);
  return factory;
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  g_mutex_init(&lock);

  pipeline = gst_parse_launch(
      "videotestsrc is-live=true pattern=smpte ! videoconvert ! "
      "video/x-raw,width=1280,height=720,framerate=30/1 ! tee name=t "
      "t. ! queue ! valve name=vhd ! x264enc speed-preset=ultrafast "
      "tune=zerolatency key-int-max=30 bitrate=2500 ! "
      "appsink name=ahd emit-signals=true sync=false "
      "t. ! queue ! valve name=vsd ! x264enc speed-preset=ultrafast "
      "tune=zerolatency key-int-max=30 bitrate=1200 ! "
      "appsink name=asd emit-signals=true sync=false",
      NULL);
  if (pipeline == NULL) {
    g_printerr("[bridge] pipeline parse failed\n");
    return 1;
  }

  profiles[0].mount = "hd";
  profiles[0].valve = gst_bin_get_by_name(GST_BIN(pipeline), "vhd");
  GstElement *asink_hd = gst_bin_get_by_name(GST_BIN(pipeline), "ahd");
  g_signal_connect(asink_hd, "new-sample", G_CALLBACK(on_new_sample),
                   &profiles[0]);
  gst_object_unref(asink_hd);

  profiles[1].mount = "sd";
  profiles[1].valve = gst_bin_get_by_name(GST_BIN(pipeline), "vsd");
  GstElement *asink_sd = gst_bin_get_by_name(GST_BIN(pipeline), "asd");
  g_signal_connect(asink_sd, "new-sample", G_CALLBACK(on_new_sample),
                   &profiles[1]);
  gst_object_unref(asink_sd);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  GstState state = GST_STATE_VOID_PENDING;
  GstStateChangeReturn ret =
      gst_element_get_state(pipeline, &state, NULL, 5 * GST_SECOND);
  if (ret != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PLAYING) {
    g_printerr("[bridge] pipeline failed to reach PLAYING (ret=%d state=%d)\n",
               ret, state);
    return 1;
  }
  g_print("[bridge] pipeline PLAYING\n");
  g_object_set(profiles[0].valve, "drop", TRUE, NULL);
  g_object_set(profiles[1].valve, "drop", TRUE, NULL);
  g_print("[bridge] idle: both valves closed\n");

  GstRTSPServer *server = gst_rtsp_server_new();
  gst_rtsp_server_set_service(server, "8554");
  GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
  GstRTSPMediaFactory *hd = make_factory(&profiles[0]);
  GstRTSPMediaFactory *sd = make_factory(&profiles[1]);
  gst_rtsp_mount_points_add_factory(mounts, "/hd", hd);
  gst_rtsp_mount_points_add_factory(mounts, "/sd", sd);
  gst_object_unref(mounts);

  gst_rtsp_server_attach(server, NULL);
  loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add_seconds(2, on_status, NULL);
  g_timeout_add_seconds(26, on_quit, NULL);
  g_print("[bridge] serving rtsp://127.0.0.1:8554/hd and /sd\n");
  g_main_loop_run(loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  gst_object_unref(server);
  return 0;
}
