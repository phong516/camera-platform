/* lab-main-loop-refs.c — who owns a GMainLoop?
 *
 * Modes:
 *   own    — create, run, unref (the pattern lab8-rtsp-client.cpp uses)
 *   leak   — create, run, never unref (LeakSanitizer shows the bill)
 *   double — unref twice (unref is refcount math, not "stop the loop")
 *   extra  — take one extra ref, then two unrefs (the first leaves it alive)
 *
 * Build (ASan required for the leak/double demos):
 *   gcc -g -O0 -fsanitize=address,undefined \
 *       labs/lab-main-loop-refs.c -o ~/cam-recon/looprefs \
 *       $(pkg-config --cflags --libs glib-2.0)
 *
 * Run:
 *   ~/cam-recon/looprefs own
 *   ~/cam-recon/looprefs leak
 *   ~/cam-recon/looprefs double
 *   ~/cam-recon/looprefs extra
 */
#include <glib.h>
#include <stdio.h>
#include <string.h>

static gboolean quit_cb(gpointer data) {
  printf("  [run]  timer fired -> g_main_loop_quit(%p)\n", data);
  g_main_loop_quit((GMainLoop *)data);
  return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  const char *mode = (argc > 1) ? argv[1] : "own";
  printf("== mode: %s ==\n", mode);

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  printf("  [new]  loop=%p  refs we own=1\n", (void *)loop);
  g_timeout_add(100, quit_cb, loop);

  if (strcmp(mode, "extra") == 0) {
    g_main_loop_ref(loop);
    printf("  [ref]  refs we own=2\n");
  }

  g_main_loop_run(loop);
  printf("  [run]  returned; is_running=%d (object is still alive)\n",
         g_main_loop_is_running(loop));

  if (strcmp(mode, "leak") == 0) {
    printf("  [exit] skipping unref on purpose\n");
    return 0;
  }

  if (strcmp(mode, "double") == 0) {
    g_main_loop_unref(loop);
    printf("  [unref 1] refs we own=0 -> object freed here\n");
    printf("  [unref 2] calling unref again on the freed pointer\n");
    g_main_loop_unref(loop);
    return 0;
  }

  if (strcmp(mode, "extra") == 0) {
    g_main_loop_unref(loop);
    printf("  [unref 1] refs we own=1 -> still alive; is_running=%d\n",
           g_main_loop_is_running(loop));
    g_main_loop_unref(loop);
    printf("  [unref 2] refs we own=0 -> freed\n");
    return 0;
  }

  g_main_loop_unref(loop);
  printf("  [unref] refs we own=0 -> freed\n");
  return 0;
}
