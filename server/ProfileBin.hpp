#pragma once

#include <gst/gst.h>

#include "model/Profile.hpp"

/// One encoded profile branch attached to ProfileHost's tee.
///
/// The implementation owns the bin and its child elements. The bin exposes a
/// single ghost sink pad and contains queue -> valve -> videoscale ->
/// capsfilter -> x264enc -> appsink. The appsink provides encoded H.264
/// buffers to the RTSP bridge; the valve is the runtime on/off switch.
class ProfileBin
{
public:
    explicit ProfileBin(ProfileInfo profile);
    ~ProfileBin();

    ProfileBin(const ProfileBin &) = delete;
    ProfileBin &operator=(const ProfileBin &) = delete;
    ProfileBin(ProfileBin &&) = delete;
    ProfileBin &operator=(ProfileBin &&) = delete;

    /// Builds and returns the bin owned by this object.
    /// Returns nullptr when an element cannot be created or linked.
    GstElement *createElement();

    GstElement *element() const;
    GstElement *appSink() const;

    void setDrop(bool drop);
    bool isDropped() const;

    const ProfileInfo &profile() const;

private:
    ProfileInfo m_profile;
    GstElement *m_bin{nullptr};
    GstElement *m_valve{nullptr};
    GstElement *m_appSink{nullptr};
};
