#include "CameraVideoSource.hpp"

GstElement *CameraVideoSource::createElement()
{
    return gst_element_factory_make("v4l2src", "camera-source");
}