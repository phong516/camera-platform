#include <string>
#include <vector>

#include "common/Protocol.hpp"
#include "model/SystemStatus.hpp"
#include "server/Application.hpp"
#include "tests/TestSupport.hpp"

namespace
{

const ProfileInfo *findProfile(const std::vector<ProfileInfo> &profiles,
                               const std::string &name)
{
    for (const ProfileInfo &profile : profiles)
    {
        if (profile.name == name)
        {
            return &profile;
        }
    }
    return nullptr;
}

} // namespace

/// Control-plane contract for Application (server/Application.hpp), phase 2:
/// the catalog is data, the ProfileHost valve is the single source of truth
/// for enabled/disabled, setProfileEnabled() delegates to it, and profiles()
/// returns the state maintained by the host. Through CameraControl only
/// "off"/"ready" are reachable here; RtspProfileServer publishes "live" when
/// RTSP media is prepared (covered by test_profilehost + the media lifecycle).
///
/// Sections 1 and 2 exercise a constructor-only Application: the assertions
/// there must hold regardless of how the .cpp wires the host, so they check
/// return codes and catalog VALUES, not valve-derived state. Section 3 runs
/// behind initialize() and asserts the phase-2 state contract directly
/// (default off, enable -> ready). It is the executable spec for the
/// Application.cpp integration: if it fails, the .cpp does not honor the
/// contract yet (e.g. branches left open by setup, or state merged from a
/// desired record instead of the valve).
int main()
{
    // --- Catalog served as data (constructor only, no gst_init) ---------------
    {
        Application app;

        const std::vector<ProfileInfo> profiles = app.profiles();
        CHECK_EQ(profiles.size(), 4u);
        CHECK_EQ(profiles[0].name, std::string("hd"));
        CHECK_EQ(profiles[1].name, std::string("sd"));
        CHECK_EQ(profiles[2].name, std::string("low"));
        CHECK_EQ(profiles[3].name, std::string("thumb"));

        // Geometry from ARCHITECTURE section 4's table (hd/sd/low/thumb).
        CHECK_EQ(profiles[0].width, 1920u);
        CHECK_EQ(profiles[0].height, 1080u);
        CHECK_EQ(profiles[0].fps, 30u);
        CHECK_EQ(profiles[0].bitrateKbps, 4000u);
        CHECK_EQ(profiles[1].width, 1280u);
        CHECK_EQ(profiles[1].height, 720u);
        CHECK_EQ(profiles[1].fps, 30u);
        CHECK_EQ(profiles[1].bitrateKbps, 2000u);
        CHECK_EQ(profiles[2].width, 854u);
        CHECK_EQ(profiles[2].height, 480u);
        CHECK_EQ(profiles[2].fps, 30u);
        CHECK_EQ(profiles[2].bitrateKbps, 800u);
        CHECK_EQ(profiles[3].width, 640u);
        CHECK_EQ(profiles[3].height, 360u);
        CHECK_EQ(profiles[3].fps, 30u);
        CHECK_EQ(profiles[3].bitrateKbps, 300u);

        // Every profile carries an absolute URL with its mount: the client
        // contract makes the URL unconditional even before the media plane.
        for (const ProfileInfo &profile : profiles)
        {
            CHECK_EQ(profile.url, proto::rtspUrlFor(proto::kRtspPort, profile.name));
            CHECK_EQ(profile.viewers, 0u);
            CHECK_EQ(profile.lastError, std::string());
        }

        // Return codes only: pre-setup valve semantics are ProfileBin's
        // business, not something the catalog block is entitled to assert.
        CHECK(app.setProfileEnabled("hd", true));
        CHECK(!app.setProfileEnabled("missing", true));
        CHECK(!app.setProfileEnabled("", true));
        CHECK(findProfile(app.profiles(), "missing") == nullptr);
        CHECK_EQ(app.profiles().size(), 4u); // unknown names create no entry

        app.stop(); // safe before initialize(): nothing to tear down
        app.stop(); // idempotent, per the contract block
    }

    // --- Default system status, untouched by profile switches -----------------
    {
        Application app;

        CHECK(app.setProfileEnabled("low", true));

        const SystemStatus status = app.systemStatus();
        CHECK(status.network == NetworkState::Unknown);
        CHECK(status.ipAddress.empty());
        CHECK(status.cpuPercent == 0.0);
    }

    // --- Phase-2 state contract, after initialize() ----------------------------
    {
        char arg0[] = "test_application";
        char *argv[] = {arg0, nullptr};
        int argc = 1;

        Application app;
        CHECK(app.initialize(argc, argv));

        // Default: everything off once the pipeline is wired up.
        const std::vector<ProfileInfo> initial = app.profiles();
        CHECK_EQ(initial.size(), 4u);
        for (const ProfileInfo &profile : initial)
        {
            CHECK_EQ(profile.state, std::string(proto::kStateOff));
            CHECK_EQ(profile.url, proto::rtspUrlFor(proto::kRtspPort, profile.name));
        }

        // Enabling flips exactly the named profile to ready.
        CHECK(app.setProfileEnabled("hd", true));
        {
            const std::vector<ProfileInfo> after = app.profiles();
            const ProfileInfo *hd = findProfile(after, "hd");
            CHECK(hd != nullptr);
            if (hd)
            {
                CHECK_EQ(hd->state, std::string(proto::kStateReady));
            }
            for (const char *other : {"sd", "low", "thumb"})
            {
                const ProfileInfo *profile = findProfile(after, other);
                CHECK(profile != nullptr);
                if (profile)
                {
                    CHECK_EQ(profile->state, std::string(proto::kStateOff));
                }
            }
        }

        // Unknown names change nothing (unknown -> false, no entry created).
        CHECK(!app.setProfileEnabled("missing", true));
        CHECK(!app.setProfileEnabled("", true));
        CHECK_EQ(app.profiles().size(), 4u);
        CHECK(findProfile(app.profiles(), "missing") == nullptr);
        CHECK(findProfile(app.profiles(), "hd")->state == proto::kStateReady);

        // Status is host-level state, unaffected by profile switches.
        const SystemStatus status = app.systemStatus();
        CHECK(status.network == NetworkState::Unknown);
        CHECK(status.ipAddress.empty());

        // Independent toggles: each profile flips on its own.
        CHECK(app.setProfileEnabled("low", true));
        CHECK(app.setProfileEnabled("low", true)); // idempotent
        CHECK(app.setProfileEnabled("hd", false));
        {
            const std::vector<ProfileInfo> after = app.profiles();
            CHECK(findProfile(after, "low")->state == proto::kStateReady);
            CHECK(findProfile(after, "hd")->state == proto::kStateOff);
            CHECK(findProfile(after, "sd")->state == proto::kStateOff);
            CHECK(findProfile(after, "thumb")->state == proto::kStateOff);
        }

        app.stop();
        app.stop(); // idempotent: main loop and RTSP detach exactly once
    }

    TEST_RETURN();
}
