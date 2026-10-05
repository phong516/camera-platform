#include <gst/gst.h>

#include "video/VideoPipeline.hpp"
#include "video/TestVideoSource.hpp"
#include "Application.hpp"

int main(int argc, char *argv[])
{
    int rc = 0;
    {
        Application app;
        if (!app.initialize(argc, argv))
        {
            rc = -1;
        }
        else
        {
            app.run();
            app.stop();
        }
    } // ~Application -> ~VideoPipeline -> cleanup() releases every GstElement here

    // Last GStreamer call in the process: nothing that owns a GstObject is alive
    // past this line, so no unref can hit a deinitialized type system.
    if (gst_is_initialized())
        gst_deinit();

    return rc;
}
