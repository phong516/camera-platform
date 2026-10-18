#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdlib.h>

static guint count = 0;
static gboolean eos_seen = FALSE;
static gint64 start_us = 0;
static gint64 first_sample_us = -1;

static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
  GstSample *sample = gst_app_sink_pull_sample(sink);
  if (sample != NULL) {
    if (count == 0) {
      first_sample_us = g_get_monotonic_time() - start_us;
    }
    count++;
    gst_sample_unref(sample);
  }
  return GST_FLOW_OK;
}

static void on_eos(GstAppSink *sink, gpointer user_data) {
  eos_seen = TRUE;
  g_print("[client] EOS received after %u samples (%.1f s in)\n", count,
          (g_get_monotonic_time() - start_us) / 1000000.0);
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  if (argc < 3) {
    g_printerr("usage: %s rtsp://host:port/mount seconds\n", argv[0]);
    return 1;
  }
  double secs = atof(argv[2]);

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

  start_us = g_get_monotonic_time();
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_usleep((gulong)(secs * 1000000));
  gst_element_set_state(pipeline, GST_STATE_NULL);

  g_print("[client] %s: samples=%u over %.1fs => %.1f frames/s", argv[1],
          count, secs, count / secs);
  if (first_sample_us >= 0) {
    g_print(" | first frame at %.0f ms", first_sample_us / 1000.0);
  } else {
    g_print(" | first frame: NEVER");
  }
  if (eos_seen) {
    g_print(" | EOS");
  }
  g_print("\n");

  gst_object_unref(sink);
  gst_object_unref(pipeline);
  return 0;
}
