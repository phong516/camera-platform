#include "TestVideoSource.hpp"

GstElement *TestVideoSource::createElement()
{
    return gst_element_factory_make("videotestsrc", "test-source");
}