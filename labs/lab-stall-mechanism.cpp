// Where does the data flow wedge when a startup-closed valve starves a sink?
// Counts buffers arriving at each branch queue, and at the valve's sink/src pads.
// Variants: closed-at / closed-at-fakeA / single-at / closed-af / open / nonlive-at

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <cstdio>
#include <cstring>

static GMainLoop *loop = nullptr;
static GstElement *pipeline = nullptr;
static GstElement *qA = nullptr, *qB = nullptr, *valve = nullptr;
static long qa_n = 0, qb_n = 0, vin_n = 0, vout_n = 0, samples = 0;
static gint64 t_start;

static gint64 now_ms() { return (g_get_monotonic_time() - t_start) / 1000; }

static GstPadProbeReturn count_cb(GstPad *, GstPadProbeInfo *, gpointer data) {
  (*(long *)data)++;
  return GST_PAD_PROBE_OK;
}

static void attach_probe(GstElement *el, const char *padname, long *counter) {
  if (!el) return;
  GstPad *pad = gst_element_get_static_pad(el, padname);
  gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, count_cb, counter, nullptr);
  gst_object_unref(pad);
}

static guint qlevel(GstElement *q) {
  guint n = 0;
  if (q) g_object_get(q, "current-level-buffers", &n, nullptr);
  return n;
}

static GstElement *find_queue(gboolean want_encoder_branch) {
  const char *names[] = {"queue0", "queue1"};
  for (int i = 0; i < 2; i++) {
    GstElement *q = gst_bin_get_by_name(GST_BIN(pipeline), names[i]);
    if (!q) continue;
    GstPad *sp = gst_element_get_static_pad(q, "src");
    GstPad *peer = gst_pad_get_peer(sp);
    GstElement *parent = peer ? gst_pad_get_parent_element(peer) : nullptr;
    gboolean is_enc = parent && g_str_has_prefix(GST_OBJECT_NAME(parent), "x264enc");
    gst_object_unref(sp);
    if (peer) gst_object_unref(peer);
    if (parent) gst_object_unref(parent);
    if (is_enc == want_encoder_branch) return q;
    gst_object_unref(q);
  }
  return nullptr;
}

static gboolean on_probe(gpointer) {
  g_print("[t=%4ld ms] samples=%-3ld qA: recv=%-4ld level=%-3u  "
          "qB: recv=%-4ld level=%-3u  valve: in=%-4ld out=%-3ld\n",
          (long)now_ms(), samples, qa_n, qlevel(qA), qb_n, qlevel(qB), vin_n,
          vout_n);
  return G_SOURCE_REMOVE;
}

static GstFlowReturn on_new_sample(GstAppSink *, gpointer) {
  samples++;
  return GST_FLOW_OK;
}

static gboolean on_reopen(gpointer) {
  g_object_set(valve, "drop", FALSE, nullptr);
  g_print("[t=%4ld ms] valve drop=false\n", (long)now_ms());
  return G_SOURCE_REMOVE;
}

static gboolean on_finish(gpointer) {
  on_probe(nullptr);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

static void on_bus_error(GstBus *, GstMessage *msg, gpointer) {
  GError *err = nullptr;
  gchar *dbg = nullptr;
  gst_message_parse_error(msg, &err, &dbg);
  g_printerr("ERROR from %s: %s (%s)\n", GST_OBJECT_NAME(msg->src),
             err->message, dbg ? dbg : "no debug");
  g_clear_error(&err);
  g_free(dbg);
  g_main_loop_quit(loop);
}

int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  const char *variant = argc > 1 ? argv[1] : "closed-at";
  gboolean closed = strcmp(variant, "open") != 0;
  gboolean af = strcmp(variant, "closed-af") == 0;
  gboolean fake_a = strcmp(variant, "closed-at-fakeA") == 0;
  gboolean single = strcmp(variant, "single-at") == 0;
  gboolean nonlive = strcmp(variant, "nonlive-at") == 0;
  g_print("== variant: %s ==\n", variant);

  gchar *desc;
  if (single) {
    desc = g_strdup_printf(
        "videotestsrc is-live=%s name=src ! videoconvert ! "
        "video/x-raw,format=I420,width=320,height=240,framerate=30/1 ! tee name=t "
        "t. ! queue ! valve name=bv ! appsink name=as emit-signals=true",
        nonlive ? "false" : "true");
  } else {
    desc = g_strdup_printf(
        "videotestsrc is-live=%s name=src ! videoconvert ! "
        "video/x-raw,format=I420,width=320,height=240,framerate=30/1 ! tee name=t "
        "t. ! queue ! x264enc speed-preset=ultrafast tune=zerolatency "
        "! %s "
        "t. ! queue ! valve name=bv ! appsink name=as emit-signals=true",
        nonlive ? "false" : "true",
        fake_a ? "fakesink name=fsA" :
                 "filesink name=fsA location=/tmp/cam-recon/stall-a.264");
  }
  pipeline = gst_parse_launch(desc, nullptr);
  g_free(desc);

  valve = gst_bin_get_by_name(GST_BIN(pipeline), "bv");
  GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "as");
  g_object_set(valve, "drop", closed ? TRUE : FALSE, nullptr);
  g_object_set(appsink, "async", af ? FALSE : TRUE, nullptr);
  g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

  qA = single ? nullptr : find_queue(TRUE);
  qB = find_queue(FALSE);
  attach_probe(qA, "sink", &qa_n);
  attach_probe(qB, "sink", &qb_n);
  attach_probe(valve, "sink", &vin_n);
  attach_probe(valve, "src", &vout_n);

  GstBus *bus = gst_element_get_bus(pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message::error", G_CALLBACK(on_bus_error), nullptr);
  gst_object_unref(bus);

  t_start = g_get_monotonic_time();
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_print("set_state(PLAYING): valve %s, appsink async=%s, branchA=%s\n",
          closed ? "CLOSED" : "open", af ? "false" : "true",
          single ? "none" : (fake_a ? "fakesink" : "filesink"));

  loop = g_main_loop_new(nullptr, FALSE);
  g_timeout_add(1000, on_probe, nullptr);
  g_timeout_add(2000, on_probe, nullptr);
  if (closed) g_timeout_add(2100, on_reopen, nullptr);
  g_timeout_add(4000, on_probe, nullptr);
  g_timeout_add(5000, on_finish, nullptr);
  g_main_loop_run(loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  return 0;
}
