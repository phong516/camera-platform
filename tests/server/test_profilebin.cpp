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
    CHECK(bin.createElement() != nullptr);
    CHECK(!bin.isDropped());

    bin.setDrop(true);
    CHECK(bin.isDropped());

    bin.setDrop(false);
    CHECK(!bin.isDropped());

    TEST_RETURN();
}