#pragma once

#include "VideoSource.hpp"

class CameraVideoSource : public VideoSource
{
public:
    CameraVideoSource() = default;
    ~CameraVideoSource() override = default;

    GstElement *createElement() override;

private:
    GstElement *m_source{nullptr};

    CameraVideoSource(const CameraVideoSource &) = delete;
    CameraVideoSource &operator=(const CameraVideoSource &) = delete;

    CameraVideoSource(CameraVideoSource &&) = delete;
    CameraVideoSource &operator=(CameraVideoSource &&) = delete;
};