#pragma once

#include "VideoSource.hpp"

class TestVideoSource : public VideoSource
{
public:
    TestVideoSource() = default;
    ~TestVideoSource() override = default;

    GstElement *createElement() override;

private:
    GstElement *m_source{nullptr};

    TestVideoSource(const TestVideoSource &) = delete;
    TestVideoSource &operator=(const TestVideoSource &) = delete;

    TestVideoSource(TestVideoSource &&) = delete;
    TestVideoSource &operator=(TestVideoSource &&) = delete;
};