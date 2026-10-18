// Lab 3 - ProfileBin: a reusable branch as a GstBin with a ghost sink pad.
// Mirrors the chain planned in server/ProfileBin: queue ! valve ! videoscale
// ! capsfilter ! x264enc ! <sink>  (filesink stands in for appsink here).
//
// Build:  g++ -Wall -O2 -o lab3 lab3-profile-bin.cpp $(pkg-config --cflags --libs gstreamer-1.0)
// Run:    ./lab3
//
// Timeline: play 3 s -> drop the branch at t=3 (valve drop=true) -> observe
// frozen output until t=6 -> quit.

#include <gst/gst.h>
#include <glib/gstdio.h>
#include <cstdio>

static const char *kOut = "/tmp/cam-recon/lab3-out.264";

static long file_size(const char *path) {
  GStatBuf st;
  return g_stat(path, &st) == 0 ? (long)st.st_size : -1;
}

// --- ProfileBin: one profile branch, self-contained ------------------------
static GstElement *profile_bin_new(const char *name, int width, int height,
                                   int kbps, const char *out_path) {
  GstElement *bin = gst_bin_new(name);

  GstElement *queue = gst_element_factory_make("queue", "pq");
  GstElement *valve = gst_element_factory_make("valve", "pvalve");
  GstElement *scale = gst_element_factory_make("videoscale", "pscale");
  GstElement *filter = gst_element_factory_make("capsfilter", "pcaps");
  GstElement *enc = gst_element_factory_make("x264enc", "penc");
  GstElement *sink = gst_element_factory_make("filesink", "psink");

  gchar *caps = g_strdup_printf(
      "video/x-raw,format=I420,width=%d,height=%d,framerate=30/1", width,
      height);
  g_object_set(filter, "caps", gst_caps_from_string(caps), nullptr);
  g_free(caps);
  gst_util_set_object_arg(G_OBJECT(enc), "speed-preset", "ultrafast");
  gst_util_set_object_arg(G_OBJECT(enc), "tune", "zerolatency");
  g_object_set(enc, "key-int-max", 30, "bitrate", kbps, nullptr);
  g_object_set(sink, "location", out_path, nullptr);

  gst_bin_add_many(GST_BIN(bin), queue, valve, scale, filter, enc, sink, nullptr);
  if (!gst_element_link_many(queue, valve, scale, filter, enc, sink, nullptr)) {
    g_printerr("failed to link internal chain\n");
    return nullptr;
  }

  // The bin pretends to be one element: expose the queue's sink pad as "sink".
  GstPad *q_sink = gst_element_get_static_pad(queue, "sink");
  GstPad *ghost = gst_ghost_pad_new("sink", q_sink);
  gst_object_unref(q_sink);
  gst_element_add_pad(bin, ghost);
  gst_element_no_more_pads(bin);

  return bin;
}

// --- host pipeline: source ! tee, one profile bin attached -----------------
static GstElement *build_host(GstElement **tee_out, GstElement **bin_out) {
  GstElement *pipeline = gst_pipeline_new("host");

  GstElement *src = gst_element_factory_make("videotestsrc", "src");
  GstElement *conv = gst_element_factory_make("videoconvert", "conv");
  GstElement *capsf = gst_element_factory_make("capsfilter", "srccaps");
  GstElement *tee = gst_element_factory_make("tee", "tee");

  g_object_set(src, "is-live", TRUE, nullptr);
  g_object_set(capsf, "caps",
               gst_caps_from_string("video/x-raw,format=I420,width=1280,"
                                    "height=720,framerate=30/1"),
               nullptr);

  gst_bin_add_many(GST_BIN(pipeline), src, conv, capsf, tee, nullptr);
  gst_element_link_many(src, conv, capsf, tee, nullptr);

  GstElement *profile = profile_bin_new("hd", 1280, 720, 2500, kOut);
  gst_bin_add(GST_BIN(pipeline), profile);

  // tee's src pads are request pads: ask for one, link it to the ghost pad.
  GstPad *tee_pad = gst_element_request_pad_simple(tee, "src_%u");
  GstPad *bin_pad = gst_element_get_static_pad(profile, "sink");
  if (gst_pad_link(tee_pad, bin_pad) != GST_PAD_LINK_OK)
    g_printerr("failed to link tee -> profile bin\n");
  gst_object_unref(tee_pad);
  gst_object_unref(bin_pad);

  *tee_out = tee;
  *bin_out = profile;
  return pipeline;
}

// --- runtime control ---------------------------------------------------------
static GMainLoop *loop = nullptr;
static GstElement *g_pipeline = nullptr;

static gboolean on_drop(gpointer data) {
  GstElement *bin = GST_ELEMENT(data);
  GstElement *valve = gst_bin_get_by_name(GST_BIN(bin), "pvalve");
  g_object_set(valve, "drop", TRUE, nullptr);
  gst_object_unref(valve);
  g_print("[t=3s] valve.drop = true        file=%ld B\n", file_size(kOut));
  return G_SOURCE_REMOVE;
}

static gboolean on_check(gpointer) {
  g_print("[t=5s] (branch dropped)        file=%ld B\n", file_size(kOut));
  return G_SOURCE_REMOVE;
}

static gboolean on_finish(gpointer) {
  g_print("[t=6s] quitting                file=%ld B\n", file_size(kOut));
  gst_element_set_state(g_pipeline, GST_STATE_NULL);
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
  remove(kOut);

  GstElement *tee = nullptr, *bin = nullptr;
  g_pipeline = build_host(&tee, &bin);

  GstBus *bus = gst_element_get_bus(g_pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message::error", G_CALLBACK(on_bus_error), nullptr);
  gst_object_unref(bus);

  gst_element_set_state(g_pipeline, GST_STATE_PLAYING);

  loop = g_main_loop_new(nullptr, FALSE);
  g_timeout_add(3000, on_drop, bin);
  g_timeout_add(5000, on_check, nullptr);
  g_timeout_add(6000, on_finish, nullptr);
  g_main_loop_run(loop);

  gst_element_set_state(g_pipeline, GST_STATE_NULL);
  gst_object_unref(g_pipeline);
  g_main_loop_unref(loop);
  return 0;
}
