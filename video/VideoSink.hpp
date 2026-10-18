#pragma once

#include <gst/gst.h>

/// Seam for the last element of the pipeline, mirroring VideoSource at the
/// front. VideoPipeline builds source, converter and capsfilter itself, but
/// delegates the sink because that is what actually varies by environment:
/// autovideosink in the app (display), fakesink in the tests (headless box).
///
/// Two adapters = a real seam: AutoVideoSink lives here (the prod default);
/// the test adapter (FakeSink, fakesink element) lives with the tests in
/// tests/video/FakeSink.hpp, not inside the video library.
///
/// Callers that don't care never learn this seam exists: VideoPipeline falls
/// back to AutoVideoSink when setVideoSink() was never called.
class VideoSink
{
public:
    virtual ~VideoSink() = default;

    /// Called once by setupPipeline(); the returned element is embedded in,
    /// and thereafter owned by, the pipeline. May be unnamed (nullptr name)
    /// or carry one, e.g. "video-sink"; VideoPipeline uses it as-is.
    virtual GstElement *createElement() = 0;
};

/// Prod adapter: the display sink (README Phase 1, videotestsrc -> screen).
class AutoVideoSink : public VideoSink
{
public:
    GstElement *createElement() override
    {
        return gst_element_factory_make("autovideosink", "video-sink");
    }
};
