#pragma once

#include <gst/gst.h>

#include "video/VideoSink.hpp"

/// Test adapter for the VideoSink seam: a sink that needs no display, so the
/// VideoPipeline test can cross the real play/pause path on a headless box.
/// The prod counterpart (AutoVideoSink) lives in video/VideoSink.hpp; the
/// test adapter lives with the tests, not inside the video library.
class FakeSink : public VideoSink
{
public:
    GstElement *createElement() override
    {
        return gst_element_factory_make("fakesink", "fake-sink");
    }
};
