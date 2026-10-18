// Lab 4 - ProfileHost: ONE source, TWO profile bins on a tee, controlled at
// runtime. This is the shape of server/ProfileHost (Phase 2).
//
// Build:  g++ -Wall -O2 -o lab4 lab4-profile-host.cpp $(pkg-config --cflags --libs gstreamer-1.0)
// Run:    ./lab4
//
// Timeline: t=3 drop sd (valve) -> t=6 resume sd -> t=9 quit. hd keeps
// running the whole time: dropping one branch must not disturb the other.

#include <gst/gst.h>
#include <glib/gstdio.h>
#include <cstdio>

struct ProfileCfg {
  const char *name;
  int width, height, kbps;
  const char *out;
};

static const ProfileCfg kProfiles[] = {
    {"hd", 1280, 720, 2500, "/tmp/cam-recon/lab4-hd.264"},
    {"sd", 854, 480, 1200, "/tmp/cam-recon/lab4-sd.264"},
};

static long file_size(const char *path) {
  GStatBuf st;
  return g_stat(path, &st) == 0 ? (long)st.st_size : -1;
}

static void print_sizes(const char *tag) {
  g_print("[%s] hd=%8ld B  sd=%8ld B\n", tag, file_size(kProfiles[0].out),
          file_size(kProfiles[1].out));
}

// --- ProfileBin builder (same shape as lab 3) ------------------------------
static GstElement *profile_bin_new(const ProfileCfg &cfg) {
  GstElement *bin = gst_bin_new(cfg.name);

  GstElement *queue = gst_element_factory_make("queue", "pq");
  GstElement *valve = gst_element_factory_make("valve", "pvalve");
  GstElement *scale = gst_element_factory_make("videoscale", "pscale");
  GstElement *filter = gst_element_factory_make("capsfilter", "pcaps");
  GstElement *enc = gst_element_factory_make("x264enc", "penc");
  GstElement *sink = gst_element_factory_make("filesink", "psink");

  gchar *caps = g_strdup_printf(
      "video/x-raw,format=I420,width=%d,height=%d,framerate=30/1",
      cfg.width, cfg.height);
  g_object_set(filter, "caps", gst_caps_from_string(caps), nullptr);
  g_free(caps);
  gst_util_set_object_arg(G_OBJECT(enc), "speed-preset", "ultrafast");
  gst_util_set_object_arg(G_OBJECT(enc), "tune", "zerolatency");
  g_object_set(enc, "key-int-max", 30, "bitrate", cfg.kbps, nullptr);
  g_object_set(sink, "location", cfg.out, nullptr);

  gst_bin_add_many(GST_BIN(bin), queue, valve, scale, filter, enc, sink, nullptr);
  if (!gst_element_link_many(queue, valve, scale, filter, enc, sink, nullptr)) {
    g_printerr("failed to link %s chain\n", cfg.name);
    return nullptr;
  }

  GstPad *q_sink = gst_element_get_static_pad(queue, "sink");
  GstPad *ghost = gst_ghost_pad_new("sink", q_sink);
  gst_object_unref(q_sink);
  gst_element_add_pad(bin, ghost);
  gst_element_no_more_pads(bin);
  return bin;
}

// --- host: source ! tee, both bins attached --------------------------------
static GstElement *build_host(GstElement **bins) {
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

  for (int i = 0; i < 2; i++) {
    bins[i] = profile_bin_new(kProfiles[i]);
    gst_bin_add(GST_BIN(pipeline), bins[i]);
    GstPad *tee_pad = gst_element_request_pad_simple(tee, "src_%u");
    GstPad *bin_pad = gst_element_get_static_pad(bins[i], "sink");
    if (gst_pad_link(tee_pad, bin_pad) != GST_PAD_LINK_OK)
      g_printerr("failed to link tee -> %s\n", kProfiles[i].name);
    gst_object_unref(tee_pad);
    gst_object_unref(bin_pad);
  }
  return pipeline;
}

// --- runtime control ---------------------------------------------------------
static GMainLoop *loop = nullptr;
static GstElement *g_pipeline = nullptr;
static GstElement *g_bins[2] = {nullptr, nullptr};

static void set_drop(int i, gboolean drop) {
  GstElement *valve = gst_bin_get_by_name(GST_BIN(g_bins[i]), "pvalve");
  g_object_set(valve, "drop", drop, nullptr);
  gst_object_unref(valve);
}

static gboolean on_drop_sd(gpointer) {
  print_sizes("t=3s before drop sd");
  set_drop(1, TRUE);
  g_print("[t=3s] sd valve.drop = true  (sd frozen from here)\n");
  return G_SOURCE_REMOVE;
}

static gboolean on_mid(gpointer) {
  print_sizes("t=5s sd dropped  ");
  return G_SOURCE_REMOVE;
}

static gboolean on_resume_sd(gpointer) {
  print_sizes("t=6s before resume ");
  set_drop(1, FALSE);
  g_print("[t=6s] sd valve.drop = false (sd resumes)\n");
  return G_SOURCE_REMOVE;
}

static gboolean on_finish(gpointer) {
  print_sizes("t=9s final        ");
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
  for (int i = 0; i < 2; i++)
    remove(kProfiles[i].out);

  g_pipeline = build_host(g_bins);

  GstBus *bus = gst_element_get_bus(g_pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message::error", G_CALLBACK(on_bus_error), nullptr);
  gst_object_unref(bus);

  gst_element_set_state(g_pipeline, GST_STATE_PLAYING);

  loop = g_main_loop_new(nullptr, FALSE);
  g_timeout_add(3000, on_drop_sd, nullptr);
  g_timeout_add(5000, on_mid, nullptr);
  g_timeout_add(6000, on_resume_sd, nullptr);
  g_timeout_add(9000, on_finish, nullptr);
  g_main_loop_run(loop);

  gst_element_set_state(g_pipeline, GST_STATE_NULL);
  gst_object_unref(g_pipeline);
  g_main_loop_unref(loop);
  return 0;
}
