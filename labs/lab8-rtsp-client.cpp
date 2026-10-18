#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <stdlib.h>

static guint count = 0;
static gboolean eos_seen = FALSE;
static gint64 t0;
static GMainLoop *mloop;

static gint64 elapsed_ms(void) { return (g_get_monotonic_time() - t0) / 1000; }

static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
  GstSample *sample = gst_app_sink_pull_sample(sink);
  if (sample != NULL) {
    count++;
    gst_sample_unref(sample);
  }
  return GST_FLOW_OK;
}

static gboolean quit_after_eos(gpointer d) {
  g_main_loop_quit((GMainLoop *)d);
  return G_SOURCE_REMOVE;
}

static void on_eos(GstAppSink *sink, gpointer user_data) {
  eos_seen = TRUE;
  g_print("[+%5lldms] EOS received after %u samples\n", elapsed_ms(), count);
  g_timeout_add(200, quit_after_eos, mloop);
}

static gboolean report(gpointer d) {
  g_print("[+%5lldms] samples=%u%s\n", elapsed_ms(), count,
          eos_seen ? "  [EOS - stream ended]" : "");
  return G_SOURCE_CONTINUE;
}

static gboolean quit_timeout(gpointer d) {
  g_main_loop_quit((GMainLoop *)d);
  return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  gst_init(&argc, &argv);
  if (argc < 3) {
    g_printerr("usage: %s rtsp://host:port/mount seconds\n", argv[0]);
    return 1;
  }
  double secs = atof(argv[2]);
  t0 = g_get_monotonic_time();

  gchar *desc = g_strdup_printf(
      "rtspsrc location=%s latency=0 protocols=tcp ! rtph264depay ! "
      "h264parse ! appsink emit-signals=true sync=false name=sink",
      argv[1]);
  GstElement *pipeline = gst_parse_launch(desc, NULL);
  g_free(desc);
  if (pipeline == NULL) {
    g_printerr("pipeline parse failed\n");
    return 1;
  }
  GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
  g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample), NULL);
  g_signal_connect(sink, "eos", G_CALLBACK(on_eos), NULL);
  gst_object_unref(sink);

  GMainLoop *mloop_local = g_main_loop_new(NULL, FALSE);
  mloop = mloop_local;
  g_timeout_add(500, report, NULL);
  g_timeout_add((guint)(secs * 1000), quit_timeout, mloop);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_main_loop_run(mloop);
  g_main_loop_unref(mloop_local);

  g_print("[+%5lldms] done: %u samples total\n", elapsed_ms(), count);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
