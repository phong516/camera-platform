#include <memory>

#include <gst/gst.h>

#include "tests/video/FakeSink.hpp"
#include "tests/TestSupport.hpp"
#include "video/TestVideoSource.hpp"
#include "video/VideoPipeline.hpp"

/// Exercises VideoPipeline through its interface only (tests/video/FakeSink.hpp
/// supplies the display-free sink, so the play path IS crossed here - the old
/// guard-only test stopped there because autovideosink needs a display).
///
/// What is asserted, per the VideoPipeline.hpp contract:
///   - every operation answers PipelineOutcome; failures carry a non-empty
///     `error` and produce no g_critical log noise;
///   - setup refuses a missing source and refuses re-entrant setup;
///   - the default AutoVideoSink adapter is active until setVideoSink();
///   - setCaps() stores pre-setup and applies live post-setup;
///   - currentPosition()/duration() return the -1 sentinel outside PLAYING/PAUSED;
///   - the source factory is retained across stop(); setupPipeline() rebuilds;
///   - attachBusWatch() requires a set-up pipeline and a real callback, and a
///     second attach replaces the watch.
/// Not asserted here: an event actually FIRING (needs a driven GMainContext
/// loop and is timing-sensitive); the Application exercises that path.
int main()
{
    gst_init(nullptr, nullptr);

    // --- default-sink instance: the AutoVideoSink fallback path -------------
    // Setup is performed but never played, so headlessness is preserved.
    {
        VideoPipeline fresh;
        VideoCaps caps;
        caps.format = "I420";
        caps.width = 1280;
        caps.height = 720;
        caps.fps_num = 25;
        caps.fps_denom = 1;

        CHECK(fresh.setVideoSource(std::make_unique<TestVideoSource>()).ok);
        CHECK(fresh.setCaps(caps).ok); // pre-setup: stored, no pipeline yet
        CHECK(fresh.setupPipeline().ok);

        // Re-entrant setup must be refused, with a reason.
        const PipelineOutcome again = fresh.setupPipeline();
        CHECK(!again.ok);
        CHECK(!again.error.empty());

        // State guards answer failures, never log-and-lie.
        CHECK(!fresh.pause().ok); // never entered PLAYING
        CHECK(!fresh.seek(0).ok);
        CHECK(fresh.currentPosition() == -1);
        CHECK(fresh.duration() == -1);
    }

    // --- bus watch refuses a pipeline that is not set up --------------------
    {
        GMainContext *context = g_main_context_new();
        VideoPipeline bare;
        const PipelineOutcome watch = bare.attachBusWatch(
            context, [](PipelineEvent, const std::string &) {});
        CHECK(!watch.ok);
        CHECK(!watch.error.empty());
        g_main_context_unref(context);
    }

    // --- full lifecycle with the FakeSink adapter ---------------------------
    {
        VideoPipeline pipeline;
        const VideoCaps liveCaps;
        int eventCount = 0;

        // No source yet: refused, reason travels in the outcome.
        const PipelineOutcome abandoned = pipeline.setupPipeline();
        CHECK(!abandoned.ok);
        CHECK(!abandoned.error.empty());

        CHECK(pipeline.setVideoSource(std::make_unique<TestVideoSource>()).ok);
        CHECK(pipeline.setVideoSink(std::make_unique<FakeSink>()).ok);
        CHECK(pipeline.setupPipeline().ok);

        // A watch without a callback is refused...
        const PipelineOutcome noCallback =
            pipeline.attachBusWatch(g_main_context_default(), PipelineEventCallback{});
        CHECK(!noCallback.ok);

        // ...a real one installs, and a later attach replaces it - both
        // answer ok.
        GMainContext *context = g_main_context_new();
        CHECK(pipeline.attachBusWatch(
                  context, [&eventCount](PipelineEvent, const std::string &) { ++eventCount; })
                  .ok);
        CHECK(pipeline.attachBusWatch(
                  context, [](PipelineEvent, const std::string &) {})
                  .ok);

        // The play path for real - fakesink needs no display.
        CHECK(pipeline.play().ok);
        CHECK(pipeline.pause().ok); // legal only from PLAYING
        CHECK(pipeline.play().ok);  // resume from PAUSED
        CHECK(pipeline.setCaps(liveCaps).ok); // applied live post-setup

        // stop() tears down; the retained source factory rebuilds.
        CHECK(pipeline.stop().ok);
        CHECK(pipeline.setupPipeline().ok);
        CHECK(pipeline.stop().ok);

        // The context was never iterated, so no event may have fired.
        CHECK_EQ(eventCount, 0);

        g_main_context_unref(context);
    }

    TEST_RETURN();
}
