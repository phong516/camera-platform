#pragma once

#include <gst/gst.h>

class VideoSource
{
public:
    virtual ~VideoSource() = default;
    virtual GstElement *createElement() = 0;
};