#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "model/Profile.hpp"
#include "video/VideoPipeline.hpp"
#include "video/VideoSource.hpp"
#include "ProfileBin.hpp"


/// Shared capture pipeline containing one tee branch per encoding profile.
///
/// ProfileHost owns the source, tee, and ProfileBin instances. Callers control
/// profiles by name and receive profile metadata or a borrowed appsink for the
/// RTSP adapter; they do not manage branch elements directly. The host keeps
/// the pipeline PLAYING and changes profile activity through valve.drop; it
/// never changes the state of an individual branch.
class ProfileHost
{
public:
    explicit ProfileHost(std::vector<ProfileInfo> profiles);
    ~ProfileHost();

    ProfileHost(const ProfileHost &) = delete;
    ProfileHost &operator=(const ProfileHost &) = delete;
    ProfileHost(ProfileHost &&) = delete;
    ProfileHost &operator=(ProfileHost &&) = delete;

    PipelineOutcome setVideoSource(std::unique_ptr<VideoSource> source);
    PipelineOutcome setupPipeline();
    PipelineOutcome play();
    PipelineOutcome stop();

    PipelineOutcome setProfileEnabled(const std::string &name, bool enabled);
    bool profileEnabled(const std::string &name) const;

    /// PUSH events: installs a bus watch on `context` and reports every
    /// ERROR/WARNING/EOS through `callback` on that context. Requires a set-up
    /// pipeline; replaces any previous watch + callback. The watch follows the
    /// bus through stop()/play(), and is detached by the destructor's
    /// cleanup() before the pipeline element goes away - no event can fire
    /// mid-teardown.
    PipelineOutcome attachBusWatch(GMainContext *context, PipelineEventCallback callback);
    void detachBusWatch();

    /// Returns profile configuration as values, independent of branch lifetime.
    std::vector<ProfileInfo> profiles() const;

    /// Returns a borrowed appsink after setup, or nullptr if setup has not
    /// completed or the profile name is unknown. Valid only while this host
    /// remains alive.
    GstAppSink *appSinkForProfile(const std::string &name) const;

private:
    std::unique_ptr<VideoSource> m_videoSource {nullptr};
    std::vector<std::unique_ptr<ProfileBin>> m_profileBins;

    GstElement *m_pipeline{nullptr};
    GstElement *m_source{nullptr};
    GstElement *m_videoConvert{nullptr};
    GstElement *m_sourceCaps{nullptr};
    GstElement *m_tee{nullptr};

    void cleanup();
    bool m_isLinked {false};

    // Bus watch (mirrors VideoPipeline's): plain-C++ events pushed to the
    // caller's context, so Application never touches GstMessage/GstBus.
    PipelineEventCallback m_appCallback{nullptr};
    GSource *m_busSource{nullptr};

    using MessageParse = std::function<void(GstMessage *, GError **, gchar **)>;

    static gboolean onBusMessageStatic(gpointer userData);
    void handleBusMessage(GstMessage *msg);
    static std::string parseMessage(GstMessage *msg, MessageParse parser);
};
