#include <gst/gst.h>

#include "server/ProfileBin.hpp"
#include "tests/TestSupport.hpp"

int main()
{
   gst_init(nullptr, nullptr);

    ProfileInfo profile;
    profile.name = "hd";
    profile.width = 1280;
    profile.height = 720;
    profile.bitrateKbps = 2500;

    ProfileBin bin(profile);
    CHECK(bin.profile().name == "hd");
    CHECK(bin.profile().state == proto::kStateOff);
    CHECK(bin.setState(proto::kStateReady));
    CHECK(bin.profile().state == proto::kStateReady);
    CHECK(bin.setState(proto::kStateLive));
    CHECK(bin.profile().state == proto::kStateLive);
    CHECK(bin.setState(proto::kStateReady));
    CHECK(bin.profile().state == proto::kStateReady);
    CHECK(bin.setState(proto::kStateOff));
    CHECK(bin.profile().state == proto::kStateOff);
    CHECK(bin.setState(proto::kStateError, "encoder failed"));
    CHECK(bin.profile().state == proto::kStateError);
    CHECK(bin.profile().lastError == "encoder failed");
    CHECK(bin.setState(proto::kStateReady));
    CHECK(bin.profile().state == proto::kStateReady);
    CHECK(bin.profile().lastError.empty());
    CHECK(!bin.setState("unknown"));
    CHECK(bin.profile().state == proto::kStateReady);
    CHECK(bin.createElement() != nullptr);
    CHECK(!bin.isDropped());

    bin.setDrop(true);
    CHECK(bin.isDropped());

    bin.setDrop(false);
    CHECK(!bin.isDropped());

    TEST_RETURN();
}
