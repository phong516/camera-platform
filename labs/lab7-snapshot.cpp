#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>

static GstElement *pipeline;
static GstElement *valve;
static GstElement *snap_sink;
static GMainLoop *loop;
static gint64 t0;
static gint64 open_time;
static gboolean waiting = FALSE;
static guint snapshot_id = 0;
static guint timeouts = 0;

static gint64 elapsed_ms(void) { return (g_get_monotonic_time() - t0) / 1000; }

static void request_snapshot(void) {
  snapshot_id++;
  waiting = TRUE;
  open_time = g_get_monotonic_time();
  g_object_set(valve, "drop", FALSE, NULL);
  g_print("[+%5lldms] snapshot %u: valve opened (drop=false)\n", elapsed_ms(),
          snapshot_id);
}

static gboolean poll_snapshot(gpointer d) {
  if (!waiting)
    return G_SOURCE_CONTINUE;
  GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(snap_sink), 0);
  if (sample != NULL) {
    gint64 lat = (g_get_monotonic_time() - open_time) / 1000;
    GstBuffer *buf = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buf, &map, GST_MAP_READ)) {
      gboolean soi = map.size > 2 && map.data[0] == 0xFF && map.data[1] == 0xD8;
      g_print("[+%5lldms] snapshot %u: JPEG delivered %lldms after valve "
              "open (%u bytes, SOI %s)\n",
              elapsed_ms(), snapshot_id, (long long)lat, (guint)map.size,
              soi ? "ok" : "MISSING");
      gst_buffer_unmap(buf, &map);
    }
    gst_sample_unref(sample);
    g_object_set(valve, "drop", TRUE, NULL);
    waiting = FALSE;
  } else if ((g_get_monotonic_time() - open_time) / 1000 > 1000) {
    timeouts++;
    g_print("[+%5lldms] snapshot %u: TIMEOUT after 1s\n", elapsed_ms(),
            snapshot_id);
    g_object_set(valve, "drop", TRUE, NULL);
    waiting = FALSE;
  }
  return G_SOURCE_CONTINUE;
}

static gboolean snap1(gpointer d) { request_snapshot(); return G_SOURCE_REMOVE; }
static gboolean snap2(gpointer d) { request_snapshot(); return G_SOURCE_REMOVE; }
static gboolean snap3(gpointer d) { request_snapshot(); return G_SOURCE_REMOVE; }
static gboolean quit(gpointer d) { g_main_loop_quit(loop); return G_SOURCE_REMOVE; }

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  gst_init(&argc, &argv);
  t0 = g_get_monotonic_time();

  pipeline = gst_parse_launch(
      "videotestsrc is-live=true pattern=smpte ! videoconvert ! "
      "video/x-raw,width=1280,height=720,framerate=30/1 ! tee name=et "
      "et. ! queue ! fakesink name=live_out sync=false "
      "et. ! queue ! valve name=sv drop=true ! jpegenc quality=85 ! "
      "appsink name=a_snap max-buffers=2 drop=true async=false sync=false",
      NULL);
  if (pipeline == NULL) {
    g_printerr("pipeline parse failed\n");
    return 1;
  }
  valve = gst_bin_get_by_name(GST_BIN(pipeline), "sv");
  snap_sink = gst_bin_get_by_name(GST_BIN(pipeline), "a_snap");
  g_object_set(valve, "drop", TRUE, NULL);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  GstState state = GST_STATE_VOID_PENDING;
  if (gst_element_get_state(pipeline, &state, NULL, 5 * GST_SECOND) !=
      GST_STATE_CHANGE_SUCCESS) {
    g_printerr("pipeline failed to reach PLAYING\n");
    return 1;
  }
  GstState vstate;
  gst_element_get_state(valve, &vstate, NULL, GST_CLOCK_TIME_NONE);
  g_print("[+%5lldms] pipeline PLAYING (valve in %s, drop=true)\n",
          elapsed_ms(), gst_element_state_get_name(vstate));

  loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add(10, poll_snapshot, NULL);
  g_timeout_add_seconds(2, snap1, NULL);
  g_timeout_add_seconds(4, snap2, NULL);
  g_timeout_add_seconds(6, snap3, NULL);
  g_timeout_add_seconds(7, quit, NULL);
  g_main_loop_run(loop);

  g_print("[+%5lldms] done (%u snapshots, %u timeouts)\n", elapsed_ms(),
          snapshot_id, timeouts);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(valve);
  gst_object_unref(snap_sink);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  return 0;
}
