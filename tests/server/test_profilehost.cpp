#include <gst/gst.h>

#include <memory>
#include <string>
#include <vector>

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

    const PipelineOutcome missing = host.setProfileEnabled("missing", true);
    CHECK(!missing.ok);
    CHECK(!missing.error.empty());
    CHECK(!host.profileEnabled("missing"));

    TEST_RETURN();
}