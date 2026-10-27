#include <gst/gst.h>

#include <string>
#include <vector>

#include "server/ProfileBin.hpp"
#include "server/ProfileHost.hpp"
#include "tests/TestSupport.hpp"

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

    ProfileBin *hdBin = host.profile("hd");
    ProfileBin *sdBin = host.profile("sd");
    CHECK(hdBin != nullptr);
    CHECK(sdBin != nullptr);
    CHECK(host.profile("missing") == nullptr);

    CHECK(hdBin->createElement() != nullptr);
    CHECK(sdBin->createElement() != nullptr);
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