#include "video/VideoPipeline.hpp"
#include "video/TestVideoSource.hpp"

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    {
        std::unique_ptr<VideoSource> source = std::make_unique<TestVideoSource>();
        VideoPipeline pipeline;
        pipeline.setVideoSource(std::move(source));
        pipeline.setupPipeline();
        pipeline.playPipeline();
        g_usleep(5 * G_USEC_PER_SEC);
    }
    // Safe here: every GstObject is already destroyed. After gst_deinit(),
    // GStreamer must not be used again (no re-init, no Gst calls).
    gst_deinit();
    return 0;
}
