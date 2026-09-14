#include "TestVideoSource.hpp"

TestVideoSource::TestVideoSource()
{
    m_source = gst_element_factory_make("videotestsrc", "test-source");
}

TestVideoSource::~TestVideoSource()
{
    if (m_source)
    {
        g_object_unref(m_source);
    }
}

GstElement *TestVideoSource::getGstVideoSource()
{
    return m_source;
}