#include <gst/gst.h>

#include <memory>
#include <string>
#include <vector>

#include "common/Protocol.hpp"
#include "server/ProfileHost.hpp"
#include "tests/TestSupport.hpp"
#include "video/TestVideoSource.hpp"

int main()
{
    gst_init(nullptr, nullptr);

    ProfileInfo hd;
    hd.name = "hd";
    hd.width = 1280;
    hd.height = 720;
    hd.bitrateKbps = 2500;

    ProfileInfo sd;
    sd.name = "sd";
    sd.width = 640;
    sd.height = 360;
    sd.bitrateKbps = 900;

    ProfileHost host(std::vector<ProfileInfo>{hd, sd});

    const std::vector<ProfileInfo> profiles = host.profiles();
    CHECK(profiles.size() == 2);
    CHECK(profiles[0].name == "hd");
    CHECK(profiles[0].width == 1280);
    CHECK(profiles[1].name == "sd");
    CHECK(profiles[1].width == 640);
    CHECK(host.appSinkForProfile("hd") == nullptr);
    CHECK(host.appSinkForProfile("missing") == nullptr);

    CHECK(host.setVideoSource(std::make_unique<TestVideoSource>()).ok);
    CHECK(host.setupPipeline().ok);
    CHECK(host.appSinkForProfile("hd") != nullptr);
    CHECK(host.appSinkForProfile("sd") != nullptr);
    CHECK(host.appSinkForProfile("missing") == nullptr);
    CHECK(host.profileEnabled("hd"));
    CHECK(host.profileEnabled("sd"));

    const PipelineOutcome disabled = host.setProfileEnabled("hd", false);
    CHECK(disabled.ok);
    CHECK(!host.profileEnabled("hd"));
    CHECK(host.profileEnabled("sd"));

    const PipelineOutcome enabled = host.setProfileEnabled("hd", true);
    CHECK(enabled.ok);
    CHECK(host.profileEnabled("hd"));
    CHECK(host.profiles()[0].state == proto::kStateReady);

    // RTSP media lifecycle seam: prepared -> live, unprepared -> ready. The
    // update changes only the reported state, never the valve.
    CHECK(host.setProfileState("hd", proto::kStateLive).ok);
    CHECK(host.profiles()[0].state == proto::kStateLive);
    CHECK(host.profileEnabled("hd"));

    CHECK(host.setProfileState("hd", proto::kStateReady).ok);
    CHECK(host.profiles()[0].state == proto::kStateReady);
    CHECK(host.profileEnabled("hd"));

    // error carries lastError, and the next non-error state clears it.
    CHECK(host.setProfileState("hd", proto::kStateError, "encoder failed").ok);
    CHECK(host.profiles()[0].state == proto::kStateError);
    CHECK(host.profiles()[0].lastError == "encoder failed");

    CHECK(host.setProfileState("hd", proto::kStateReady).ok);
    CHECK(host.profiles()[0].state == proto::kStateReady);
    CHECK(host.profiles()[0].lastError.empty());

    // Unknown profile / unknown state are rejected without touching the profile.
    const PipelineOutcome missingState = host.setProfileState("missing", proto::kStateLive);
    CHECK(!missingState.ok);
    CHECK(!missingState.error.empty());

    const PipelineOutcome invalidState = host.setProfileState("hd", "unknown");
    CHECK(!invalidState.ok);
    CHECK(!invalidState.error.empty());
    CHECK(host.profiles()[0].state == proto::kStateReady);

    const PipelineOutcome missing = host.setProfileEnabled("missing", true);
    CHECK(!missing.ok);
    CHECK(!missing.error.empty());
    CHECK(!host.profileEnabled("missing"));

    TEST_RETURN();
}