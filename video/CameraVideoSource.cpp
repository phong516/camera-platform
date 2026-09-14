#include "CameraVideoSource.hpp"

CameraVideoSource::CameraVideoSource()
{
    m_source = gst_element_factory_make("v4l2src", "camera-source");
}

CameraVideoSource::~CameraVideoSource()
{
    if (m_source)
    {
        g_object_unref(m_source);
    }
}

GstElement* CameraVideoSource::getGstVideoSource()
{
    return m_source;
}