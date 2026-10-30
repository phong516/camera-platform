#include <gst/gst.h>

#include <memory>
#include <vector>

#include "server/ProfileHost.hpp"
#include "server/RtspProfileServer.hpp"
#include "tests/TestSupport.hpp"
#include "video/TestVideoSource.hpp"

int main()
{
    gst_init(nullptr, nullptr);

    ProfileInfo profile;
    profile.name = "test";
    profile.width = 640;
    profile.height = 360;
    profile.fps = 30;
    profile.bitrateKbps = 500;

    ProfileHost host(std::vector<ProfileInfo>{profile});
    CHECK(host.setVideoSource(std::make_unique<TestVideoSource>()).ok);
    const PipelineOutcome setup = host.setupPipeline();
    CHECK(setup.ok);
    if (!setup.ok)
    {
        TEST_RETURN();
    }
    CHECK(host.appSinkForProfile("test") != nullptr);

    GMainContext *context = g_main_context_new();
    {
        RtspProfileServer server(host, 0);

        const PipelineOutcome attached = server.attach(*context);
        CHECK(attached.ok);

        const PipelineOutcome attachedAgain = server.attach(*context);
        CHECK(!attachedAgain.ok);

        server.detach();
        server.detach();
    }
    g_main_context_unref(context);

    TEST_RETURN();
}