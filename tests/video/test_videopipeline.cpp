#include <memory>

#include <gst/gst.h>

#include "tests/TestSupport.hpp"
#include "video/TestVideoSource.hpp"
#include "video/VideoPipeline.hpp"

/// Exercises the already-implemented VideoPipeline lifecycle guards.
/// Intentionally never calls playPipeline(): this development box has no
/// display, so a sink-dependent assertion would be flaky.
int main()
{
    gst_init(nullptr, nullptr);

    VideoPipeline pipeline;

    // No source yet.
    CHECK(!pipeline.setupPipeline());

    CHECK(pipeline.setVideoSource(std::make_unique<TestVideoSource>()));
    CHECK(pipeline.setupPipeline());

    // Re-entrant setup and late source replacement must both be refused.
    CHECK(!pipeline.setupPipeline());
    CHECK(!pipeline.setVideoSource(std::make_unique<TestVideoSource>()));

    // Caps are stored before the pipeline is (re)built.
    VideoCaps caps;
    caps.format = "RGB";
    caps.width = 1280;
    caps.height = 720;
    caps.fps_num = 25;
    caps.fps_denom = 1;
    CHECK(pipeline.setCapsProperty(caps));

    // Queries against a live pipeline with no clock are best-effort; only the
    // state guard is asserted here.
    CHECK(pipeline.pausePipeline() == false); // never entered PLAYING

    TEST_RETURN();
}
