#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>

typedef struct {
  GstElement *bin;
  GstPad *tee_pad;
  gint64 deadline;
} Rec;

static GstElement *pipeline;
static GstElement *tee;
static GMainLoop *loop;
static Rec rec;
static gboolean rec_eos_seen = FALSE;
static guint rtsp_samples = 0;
static gint64 t0;

static gint64 elapsed_ms(void) { return (g_get_monotonic_time() - t0) / 1000; }

static GstFlowReturn on_rtsp_sample(GstAppSink *sink, gpointer user_data) {
  GstSample *sample = gst_app_sink_pull_sample(sink);
  if (sample != NULL) {
    rtsp_samples++;
    gst_sample_unref(sample);
  }
  return GST_FLOW_OK;
}

static GstPadProbeReturn on_branch_event(GstPad *pad, GstPadProbeInfo *info,
                                         gpointer user_data) {
  GstEvent *ev = GST_PAD_PROBE_INFO_EVENT(info);
  if (GST_EVENT_TYPE(ev) == GST_EVENT_EOS) {
    rec_eos_seen = TRUE;
    g_print("[+%5lldms] EOS event reached filesink -> mux finalized\n",
            elapsed_ms());
  }
  return GST_PAD_PROBE_OK;
}

static void rec_teardown(void) {
  gst_element_set_state(rec.bin, GST_STATE_NULL);
  gst_bin_remove(GST_BIN(pipeline), rec.bin);
}

static gboolean poll_finish(gpointer d) {
  if (rec_eos_seen) {
    rec_teardown();
    return G_SOURCE_REMOVE;
  }
  GstBus *bus = gst_element_get_bus(pipeline);
  GstMessage *msg =
      gst_bus_pop_filtered(bus, (GstMessageType)GST_MESSAGE_ERROR);
  gst_object_unref(bus);
  if (msg != NULL) {
    GError *err = NULL;
    gchar *dbg = NULL;
    gst_message_parse_error(msg, &err, &dbg);
    g_print("[+%5lldms] BUS ERROR: %s (%s)\n", elapsed_ms(), err->message, dbg);
    g_error_free(err);
    g_free(dbg);
    gst_message_unref(msg);
    rec_teardown();
    return G_SOURCE_REMOVE;
  }
  if (g_get_monotonic_time() / 1000 > rec.deadline) {
    g_print("[+%5lldms] EOS never reached filesink - NULL-state fallback\n",
            elapsed_ms());
    rec_teardown();
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

static void rec_start(const char *location, gboolean faststart) {
  rec_eos_seen = FALSE;
  rec.bin = gst_bin_new("recbin");
  GstElement *queue = gst_element_factory_make("queue", NULL);
  GstElement *parse = gst_element_factory_make("h264parse", NULL);
  GstElement *mux = gst_element_factory_make("mp4mux", NULL);
  GstElement *filesink = gst_element_factory_make("filesink", NULL);
  g_object_set(filesink, "location", location, NULL);
  g_object_set(mux, "faststart", faststart, NULL);
  gst_bin_add_many(GST_BIN(rec.bin), queue, parse, mux, filesink, NULL);
  gst_element_link_many(queue, parse, mux, filesink, NULL);

  gst_pad_add_probe(gst_element_get_static_pad(filesink, "sink"),
                    GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, on_branch_event, NULL,
                    NULL);

  GstPad *sinkpad = gst_element_get_static_pad(queue, "sink");
  gst_element_add_pad(rec.bin, gst_ghost_pad_new("sink", sinkpad));
  gst_object_unref(sinkpad);

  gst_bin_add(GST_BIN(pipeline), rec.bin);
  gst_element_sync_state_with_parent(rec.bin);

  rec.tee_pad = gst_element_request_pad_simple(tee, "src_%u");
  GstPad *bin_pad = gst_element_get_static_pad(rec.bin, "sink");
  gst_pad_link(rec.tee_pad, bin_pad);
  gst_object_unref(bin_pad);
  g_print("[+%5lldms] rec start -> %s (faststart=%s, rtsp consumer at %u "
          "samples)\n",
          elapsed_ms(), location, faststart ? "yes" : "no", rtsp_samples);
}

static void rec_stop(void) {
  GstPad *bin_pad = gst_element_get_static_pad(rec.bin, "sink");
  gst_pad_unlink(rec.tee_pad, bin_pad);
  gst_element_release_request_pad(tee, rec.tee_pad);
  gst_object_unref(rec.tee_pad);
  gst_pad_send_event(bin_pad, gst_event_new_eos());
  gst_object_unref(bin_pad);
  g_print("[+%5lldms] rec stop: unlinked from tee, EOS sent into branch\n",
          elapsed_ms());
  rec.deadline = g_get_monotonic_time() / 1000 + 2500;
  g_timeout_add(25, poll_finish, NULL);
}

static gboolean start1(gpointer d) { rec_start("/home/phong/cam-recon/rec1.mp4", FALSE); return G_SOURCE_REMOVE; }
static gboolean stop1(gpointer d) { rec_stop(); return G_SOURCE_REMOVE; }
static gboolean start2(gpointer d) { rec_start("/home/phong/cam-recon/rec2.mp4", TRUE); return G_SOURCE_REMOVE; }
static gboolean stop2(gpointer d) { rec_stop(); return G_SOURCE_REMOVE; }
static gboolean start3(gpointer d) { rec_start("/home/phong/cam-recon/rec_bad.mp4", FALSE); return G_SOURCE_REMOVE; }

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  gst_init(&argc, &argv);
  t0 = g_get_monotonic_time();

  pipeline = gst_parse_launch(
      "videotestsrc is-live=true pattern=smpte ! videoconvert ! "
      "video/x-raw,width=1280,height=720,framerate=30/1 ! "
      "x264enc speed-preset=ultrafast tune=zerolatency key-int-max=30 "
      "bitrate=2500 ! tee name=et "
      "et. ! queue ! appsink name=a_rtsp emit-signals=true sync=false",
      NULL);
  if (pipeline == NULL) {
    g_printerr("pipeline parse failed\n");
    return 1;
  }

  tee = gst_bin_get_by_name(GST_BIN(pipeline), "et");
  GstElement *rtsp_sink = gst_bin_get_by_name(GST_BIN(pipeline), "a_rtsp");
  g_signal_connect(rtsp_sink, "new-sample", G_CALLBACK(on_rtsp_sample), NULL);
  gst_object_unref(rtsp_sink);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  GstState state = GST_STATE_VOID_PENDING;
  if (gst_element_get_state(pipeline, &state, NULL, 5 * GST_SECOND) !=
      GST_STATE_CHANGE_SUCCESS) {
    g_printerr("pipeline failed to reach PLAYING\n");
    return 1;
  }
  g_print("[+%5lldms] pipeline PLAYING\n", elapsed_ms());

  loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add_seconds(2, start1, NULL);
  g_timeout_add_seconds(4, stop1, NULL);
  g_timeout_add_seconds(6, start2, NULL);
  g_timeout_add_seconds(8, stop2, NULL);
  g_timeout_add_seconds(10, start3, NULL);
  g_main_loop_run(loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(tee);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  return 0;
}
