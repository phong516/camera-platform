#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GstElement *appsrc;
  guint session;
  guint pushed;
  guint dropped;
  guint pushfail;
  gboolean caps_set;
  GstCaps *last_caps;
  GstBuffer *last_buf;
} Bridge;

static GstElement *producer;
static guint producer_gen = 0;
static gboolean producer_eos = FALSE;
static gboolean dead_logged = FALSE;
static gint64 last_sample_us = 0;
static guint producer_samples = 0;
static Bridge bridge;
static GMutex lock;
static GMainLoop *loop;
static gint64 t0;

static gint64 elapsed_ms(void) { return (g_get_monotonic_time() - t0) / 1000; }

static GstFlowReturn on_producer_sample(GstAppSink *sink, gpointer user_data) {
  GstSample *sample = gst_app_sink_pull_sample(sink);
  if (sample == NULL) {
    return GST_FLOW_ERROR;
  }
  GstBuffer *buffer = gst_buffer_copy(gst_sample_get_buffer(sample));
  GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

  g_mutex_lock(&lock);
  producer_samples++;
  last_sample_us = g_get_monotonic_time();
  gst_caps_replace(&bridge.last_caps, gst_sample_get_caps(sample));
  gst_buffer_replace(&bridge.last_buf, buffer);
  if (bridge.appsrc != NULL) {
    if (!bridge.caps_set) {
      gst_app_src_set_caps(GST_APP_SRC(bridge.appsrc),
                           gst_sample_get_caps(sample));
      bridge.caps_set = TRUE;
    }
    GstFlowReturn ret =
        gst_app_src_push_buffer(GST_APP_SRC(bridge.appsrc), buffer);
    if (ret == GST_FLOW_OK) {
      bridge.pushed++;
    } else {
      bridge.pushfail++;
    }
  } else {
    gst_buffer_unref(buffer);
    bridge.dropped++;
  }
  g_mutex_unlock(&lock);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

static void on_producer_eos(GstAppSink *sink, gpointer user_data) {
  g_mutex_lock(&lock);
  producer_eos = TRUE;
  g_print("[+%5lldms] producer gen=%u hit EOS after %u samples\n", elapsed_ms(),
          producer_gen, producer_samples);
  if (bridge.appsrc != NULL) {
    gst_app_src_end_of_stream(GST_APP_SRC(bridge.appsrc));
    g_print("[+%5lldms] forwarded EOS to viewers (appsrc session %u)\n",
            elapsed_ms(), bridge.session);
  } else {
    g_print("[+%5lldms] no viewers connected, nothing to forward\n",
            elapsed_ms());
  }
  g_mutex_unlock(&lock);
}

static void on_media_unprepared(GstRTSPMedia *media, gpointer user_data) {
  g_mutex_lock(&lock);
  guint pushed = bridge.pushed;
  if (bridge.appsrc != NULL) {
    gst_object_unref(bridge.appsrc);
    bridge.appsrc = NULL;
  }
  g_mutex_unlock(&lock);
  g_print("[+%5lldms] viewer left -> session %u done (pushed %u) -> appsrc "
          "unwired\n",
          elapsed_ms(), bridge.session, pushed);
}

static void on_media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media,
                               gpointer user_data) {
  GstElement *element = gst_rtsp_media_get_element(media);
  GstElement *appsrc = gst_bin_get_by_name(GST_BIN(element), "vsrc");
  gst_object_unref(element);
  g_mutex_lock(&lock);
  if (bridge.appsrc != NULL) {
    gst_object_unref(bridge.appsrc);
  }
  bridge.appsrc = appsrc;
  bridge.session++;
  bridge.pushed = 0;
  bridge.pushfail = 0;
  bridge.caps_set = FALSE;
  guint session = bridge.session;
  if (bridge.last_caps != NULL && bridge.last_buf != NULL) {
    gst_app_src_set_caps(GST_APP_SRC(appsrc), bridge.last_caps);
    bridge.caps_set = TRUE;
    gst_app_src_push_buffer(GST_APP_SRC(appsrc),
                            gst_buffer_copy(bridge.last_buf));
    g_print("[+%5lldms] pre-seeded new appsrc with latest frame\n",
            elapsed_ms());
  }
  g_mutex_unlock(&lock);
  g_signal_connect(media, "unprepared", G_CALLBACK(on_media_unprepared), NULL);
  g_print("[+%5lldms] viewer arrived -> session %u, appsrc wired (gen=%u)\n",
          elapsed_ms(), session, producer_gen);
}

static gboolean on_watchdog(gpointer user_data) {
  g_mutex_lock(&lock);
  gboolean wired = bridge.appsrc != NULL;
  gboolean eos = producer_eos;
  gboolean dead = dead_logged;
  gint64 silence_ms =
      last_sample_us ? (g_get_monotonic_time() - last_sample_us) / 1000 : -1;
  g_mutex_unlock(&lock);
  if (wired && !eos && !dead && silence_ms > 1500) {
    dead_logged = TRUE;
    g_print("[+%5lldms] WATCHDOG: SOURCE DEAD (gen=%u) - no samples for "
            "%lldms, no EOS. Viewers are frozen but still connected.\n",
            elapsed_ms(), producer_gen, (long long)silence_ms);
  }
  return G_SOURCE_CONTINUE;
}

static void produce_start(guint num_buffers) {
  producer_gen++;
  producer_samples = 0;
  producer_eos = FALSE;
  dead_logged = FALSE;
  last_sample_us = g_get_monotonic_time();
  gchar *desc = g_strdup_printf(
      "videotestsrc is-live=true pattern=smpte num-buffers=%u ! videoconvert "
      "! video/x-raw,width=1280,height=720,framerate=30/1 ! x264enc "
      "speed-preset=ultrafast tune=zerolatency key-int-max=30 bitrate=2500 ! "
      "h264parse ! appsink name=psink emit-signals=true sync=false "
      "max-buffers=2 drop=true",
      num_buffers);
  producer = gst_parse_launch(desc, NULL);
  g_free(desc);
  GstElement *psink = gst_bin_get_by_name(GST_BIN(producer), "psink");
  g_signal_connect(psink, "new-sample", G_CALLBACK(on_producer_sample), NULL);
  g_signal_connect(psink, "eos", G_CALLBACK(on_producer_eos), NULL);
  gst_object_unref(psink);
  gst_element_set_state(producer, GST_STATE_PLAYING);
  g_print("[+%5lldms] producer gen=%u started (%u buffers)\n", elapsed_ms(),
          producer_gen, num_buffers);
}

static void producer_crash(void) {
  g_print("[+%5lldms] SIMULATING SOURCE CRASH: killing producer gen=%u "
          "without EOS\n",
          elapsed_ms(), producer_gen);
  gst_element_set_state(producer, GST_STATE_NULL);
  gst_object_unref(producer);
  producer = NULL;
}

static gboolean ev_gen1(gpointer d) { produce_start(90); return G_SOURCE_REMOVE; }
static gboolean ev_gen2(gpointer d) { produce_start(100000); return G_SOURCE_REMOVE; }
static gboolean ev_crash(gpointer d) { producer_crash(); return G_SOURCE_REMOVE; }
static gboolean ev_gen3(gpointer d) { produce_start(90); return G_SOURCE_REMOVE; }
static gboolean ev_quit(gpointer d) { g_main_loop_quit(loop); return G_SOURCE_REMOVE; }

static GstRTSPMediaFactory *make_factory(void) {
  GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
  gst_rtsp_media_factory_set_launch(
      factory,
      "( appsrc name=vsrc is-live=true do-timestamp=true ! h264parse ! "
      "rtph264pay name=pay0 pt=96 config-interval=1 )");
  gst_rtsp_media_factory_set_shared(factory, TRUE);
  g_signal_connect(factory, "media-configure", G_CALLBACK(on_media_configure),
                   NULL);
  return factory;
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  gst_init(&argc, &argv);
  g_mutex_init(&lock);
  t0 = g_get_monotonic_time();

  GstRTSPServer *server = gst_rtsp_server_new();
  gst_rtsp_server_set_service(server, "8554");
  GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
  GstRTSPMediaFactory *cam = make_factory();
  gst_rtsp_mount_points_add_factory(mounts, "/cam", cam);
  gst_object_unref(mounts);
  gst_rtsp_server_attach(server, NULL);
  g_print("[+%5lldms] serving rtsp://127.0.0.1:8554/cam\n", elapsed_ms());

  loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add(250, on_watchdog, NULL);
  g_timeout_add_seconds(1, ev_gen1, NULL);
  g_timeout_add_seconds(5, ev_gen2, NULL);
  g_timeout_add_seconds(10, ev_crash, NULL);
  g_timeout_add(12500, ev_gen3, NULL);
  g_timeout_add_seconds(17, ev_quit, NULL);
  g_main_loop_run(loop);

  if (producer != NULL) {
    gst_element_set_state(producer, GST_STATE_NULL);
    gst_object_unref(producer);
  }
  gst_object_unref(server);
  g_main_loop_unref(loop);
  return 0;
}
