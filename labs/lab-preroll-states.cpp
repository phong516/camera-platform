// Probe: where exactly does a pipeline sit when a valve closed at startup
// starves a sink, in a LIVE pipeline? Queries committed + pending state of the
// pipeline and key elements, and prints every bus message.
// Variants: closed-at (appsink default async=true) / closed-af (async=false) / open.

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib/gstdio.h>
#include <cstdio>
#include <cstring>

static GMainLoop *loop = nullptr;
static GstElement *pipeline = nullptr;
static long sample_count = 0;
static gint64 t_start = 0;

static gint64 ms_now() { return (g_get_monotonic_time() - t_start) / 1000; }
static const char *sname(GstState s) { return gst_element_state_get_name(s); }

static const char *ret_name(GstStateChangeReturn r) {
  switch (r) {
    case GST_STATE_CHANGE_FAILURE: return "FAILURE";
    case GST_STATE_CHANGE_SUCCESS: return "SUCCESS";
    case GST_STATE_CHANGE_ASYNC: return "ASYNC";
    case GST_STATE_CHANGE_NO_PREROLL: return "NO_PREROLL";
    default: return "?";
  }
}

static void probe_one(const char *name, GstElement *el) {
  GstState s = GST_STATE_VOID_PENDING, p = GST_STATE_VOID_PENDING;
  GstStateChangeReturn r = gst_element_get_state(el, &s, &p, 0);
  g_print("  %-9s state=%-7s pending=%-7s (ret=%s)\n", name, sname(s), sname(p),
          ret_name(r));
}

static gboolean on_probe(gpointer) {
  g_print("[t=%ld ms] probe:\n", (long)ms_now());
  probe_one("pipeline", pipeline);
  const char *names[] = {"src", "fsA", "bv", "as"};
  for (int i = 0; i < 4; i++) {
    GstElement *el = gst_bin_get_by_name(GST_BIN(pipeline), names[i]);
    if (el) { probe_one(names[i], el); gst_object_unref(el); }
  }
  g_print("  samples=%ld\n", sample_count);
  return G_SOURCE_REMOVE;
}

static GstFlowReturn on_new_sample(GstAppSink *, gpointer) {
  sample_count++;
  return GST_FLOW_OK;
}

static gboolean on_reopen(gpointer valve) {
  g_object_set(valve, "drop", FALSE, nullptr);
  g_print("[t=%ld ms] valve drop=false\n", (long)ms_now());
  return G_SOURCE_REMOVE;
}

static gboolean on_finish(gpointer) {
  on_probe(nullptr);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

static void on_bus_message(GstBus *, GstMessage *msg, gpointer) {
  const char *src = GST_OBJECT_NAME(GST_MESSAGE_SRC(msg));
  if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_STATE_CHANGED) {
    GstState old_s, new_s, pend_s;
    gst_message_parse_state_changed(msg, &old_s, &new_s, &pend_s);
    g_print("[t=%4ld ms] bus: STATE_CHANGED %-9s %s -> %s\n", (long)ms_now(), src,
            sname(old_s), sname(new_s));
  } else {
    g_print("[t=%4ld ms] bus: %-14s from %s\n", (long)ms_now(),
            GST_MESSAGE_TYPE_NAME(msg), src);
  }
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  const char *variant = argc > 1 ? argv[1] : "closed-at";
  gboolean closed = strcmp(variant, "open") != 0;
  gboolean af = strcmp(variant, "closed-af") == 0;
  gboolean nonlive = strcmp(variant, "nonlive-at") == 0;
  g_print("== variant: %s ==\n", variant);
  remove("/tmp/cam-recon/probe-a.264");

  gchar *desc = g_strdup_printf(
      "videotestsrc is-live=%s name=src ! videoconvert ! "
      "video/x-raw,format=I420,width=320,height=240,framerate=30/1 ! tee name=t "
      "t. ! queue ! x264enc speed-preset=ultrafast tune=zerolatency "
      "! filesink name=fsA location=/tmp/cam-recon/probe-a.264 "
      "t. ! queue ! valve name=bv ! appsink name=as emit-signals=true",
      nonlive ? "false" : "true");
  pipeline = gst_parse_launch(desc, nullptr);
  g_free(desc);

  GstElement *valve = gst_bin_get_by_name(GST_BIN(pipeline), "bv");
  GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "as");
  g_object_set(valve, "drop", closed ? TRUE : FALSE, nullptr);
  g_object_set(appsink, "async", af ? FALSE : TRUE, nullptr);
  g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

  GstBus *bus = gst_element_get_bus(pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message", G_CALLBACK(on_bus_message), nullptr);
  gst_object_unref(bus);

  t_start = g_get_monotonic_time();
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_print("[t=%ld ms] set_state(PLAYING) called (valve %s, appsink async=%s)\n",
          (long)ms_now(), closed ? "CLOSED" : "open", af ? "false" : "true");

  loop = g_main_loop_new(nullptr, FALSE);
  g_timeout_add(2000, on_probe, nullptr);
  if (closed) g_timeout_add(2100, on_reopen, valve);
  g_timeout_add(4000, on_probe, nullptr);
  g_timeout_add(5000, on_finish, nullptr);
  g_main_loop_run(loop);

  gst_object_unref(valve);
  gst_object_unref(appsink);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  return 0;
}
