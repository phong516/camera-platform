// Recon lab: does async=false on an appsink rescue a valve-closed-at-startup
// branch, and can such a branch be reopened later?
// Variants: closed-af (drop=true, async=false), closed-at (drop=true, async=true),
//           open (drop=false, async=true; reference).
// Branch A (queue ! x264enc ! filesink) proves whether the pipeline flows at all.
// Branch B (queue ! valve ! appsink) counts samples; valve reopens at t=2s.

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib/gstdio.h>
#include <cstdio>
#include <cstring>

static const char *kOutA = "/tmp/cam-recon/async-a.264";
static GMainLoop *loop = nullptr;
static GstElement *pipeline = nullptr;
static long sample_count = 0;
static gint64 t_start = 0;

static long file_size(const char *path) {
  GStatBuf st;
  return g_stat(path, &st) == 0 ? (long)st.st_size : -1;
}

static gint64 ms_since_start() { return (g_get_monotonic_time() - t_start) / 1000; }

static GstFlowReturn on_new_sample(GstAppSink *, gpointer) {
  sample_count++;
  if (sample_count == 1) g_print("  first sample arrived at t=%ld ms\n", (long)ms_since_start());
  return GST_FLOW_OK;
}

static gboolean on_reopen(gpointer valve) {
  g_print("[t=2s] A=%ld B  samples=%ld  -> reopening valve\n", file_size(kOutA), sample_count);
  g_object_set(valve, "drop", FALSE, nullptr);
  return G_SOURCE_REMOVE;
}

static gboolean on_finish(gpointer) {
  g_print("[t=6s] A=%ld B  samples=%ld\n", file_size(kOutA), sample_count);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

static void on_bus_error(GstBus *, GstMessage *msg, gpointer) {
  GError *err = nullptr;
  gchar *dbg = nullptr;
  gst_message_parse_error(msg, &err, &dbg);
  g_printerr("ERROR from %s: %s (%s)\n", GST_OBJECT_NAME(msg->src), err->message,
             dbg ? dbg : "no debug");
  g_clear_error(&err);
  g_free(dbg);
  g_main_loop_quit(loop);
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  const char *variant = argc > 1 ? argv[1] : "closed-af";
  gboolean closed = strcmp(variant, "open") != 0;
  gboolean use_async_false = strcmp(variant, "closed-af") == 0;
  remove(kOutA);
  g_print("== variant: %s ==\n", variant);

  pipeline = gst_parse_launch(
      "videotestsrc is-live=true ! videoconvert ! "
      "video/x-raw,format=I420,width=320,height=240,framerate=30/1 ! tee name=t "
      "t. ! queue ! x264enc speed-preset=ultrafast tune=zerolatency "
      "! filesink location=/tmp/cam-recon/async-a.264 "
      "t. ! queue ! valve name=bv ! appsink name=as emit-signals=true",
      nullptr);

  GstElement *valve = gst_bin_get_by_name(GST_BIN(pipeline), "bv");
  GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "as");
  g_object_set(valve, "drop", closed ? TRUE : FALSE, nullptr);
  g_object_set(appsink, "async", use_async_false ? FALSE : TRUE, nullptr);
  g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

  GstBus *bus = gst_element_get_bus(pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message::error", G_CALLBACK(on_bus_error), nullptr);
  gst_object_unref(bus);

  t_start = g_get_monotonic_time();
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_print("[t=0s] state set to PLAYING (valve %s, appsink async=%s)\n",
          closed ? "CLOSED" : "open", use_async_false ? "false" : "true");

  loop = g_main_loop_new(nullptr, FALSE);
  if (closed) g_timeout_add(2000, on_reopen, valve);
  g_timeout_add(6000, on_finish, nullptr);
  g_main_loop_run(loop);

  gst_object_unref(valve);
  gst_object_unref(appsink);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  return 0;
}
