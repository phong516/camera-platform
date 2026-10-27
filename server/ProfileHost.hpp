#pragma once

#include <gst/gst.h>

#include <memory>
#include <string>
#include <vector>

#include "model/Profile.hpp"
#include "video/VideoPipeline.hpp"
#include "video/VideoSource.hpp"
#include "ProfileBin.hpp"


/// Shared capture pipeline containing one tee branch per encoding profile.
///
/// ProfileHost owns the source, tee, and ProfileBin instances. It keeps the
/// pipeline PLAYING and changes profile activity through valve.drop; it never
/// changes the state of an individual branch.
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

    GstElement *pipeline() const;
    ProfileBin *profile(const std::string &name) const;

private:
    std::unique_ptr<VideoSource> m_videoSource;
    std::vector<std::unique_ptr<ProfileBin>> m_profileBins;

    GstElement *m_pipeline{nullptr};
    GstElement *m_source{nullptr};
    GstElement *m_videoConvert{nullptr};
    GstElement *m_sourceCaps{nullptr};
    GstElement *m_tee{nullptr};

    void cleanup();
    bool m_isLinked {false};
};
