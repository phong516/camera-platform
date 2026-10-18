#include <map>
#include <string>
#include <vector>

#include "common/Protocol.hpp"
#include "model/Profile.hpp"
#include "model/SystemStatus.hpp"
#include "tests/TestSupport.hpp"
#include "web/ApiController.hpp"

/// NOT COMPILED YET: this test needs web/ApiController.cpp.
/// Uncomment the test_apicontroller block in tests/CMakeLists.txt once it exists
/// (Phase 3). This file tracks the CameraControl seam and the section 6 API
/// surface it serves (common/Protocol.hpp), so Phase 3 starts against a test,
/// not a blank page.

namespace
{
/// Fake adapter on the CameraControl seam - the same seam Application
/// implements and the tests sit on. Mirrors Application's Phase 1 contract:
/// a two-profile catalog where setProfileEnabled() only records desired state,
/// and one profile is deliberately state==error to prove "errors are data".
class FakeControl : public CameraControl
{
public:
    SystemStatus systemStatus() const override
    {
        SystemStatus status;
        status.network = NetworkState::Wireless;
        status.ipAddress = "192.168.1.20";
        return status;
    }

    std::vector<ProfileInfo> profiles() const override
    {
        ProfileInfo hd;
        hd.name = "hd";
        hd.width = 1920;
        hd.height = 1080;
        hd.fps = 30;
        hd.bitrateKbps = 4000;
        hd.url = proto::rtspUrlFor(proto::kRtspPort, "/hd");
        hd.state = m_enabled.count("hd") && m_enabled.at("hd")
                       ? proto::kStateReady
                       : proto::kStateOff;

        ProfileInfo low;
        low.name = "low";
        low.width = 854;
        low.height = 480;
        low.fps = 30;
        low.bitrateKbps = 800;
        low.url = proto::rtspUrlFor(proto::kRtspPort, "/low");
        low.state = proto::kStateError;
        low.lastError = "simulated branch failure";

        return {hd, low};
    }

    bool setProfileEnabled(const std::string &name, bool enabled) override
    {
        if (name != "hd" && name != "low")
            return false;
        m_enabled[name] = enabled; // idempotent; unchanged value still true
        return true;
    }

    bool enabled(const std::string &name) const
    {
        return m_enabled.count(name) != 0 && m_enabled.at(name);
    }

private:
    std::map<std::string, bool> m_enabled;
};

HttpRequest request(const std::string &method, const std::string &path)
{
    HttpRequest built;
    built.method = method;
    built.path = path;
    return built;
}
} // namespace

int main()
{
    FakeControl control;
    ApiController api(control);

    // --- liveness -----------------------------------------------------------
    const HttpResponse health = api.getHealth(request("GET", proto::kApiHealth));
    CHECK_EQ(health.status, 200);
    CHECK(health.body.find("\"ok\"") != std::string::npos);

    // --- /api/status: exactly the SystemStatus JSON, nothing concatenated ---
    const HttpResponse status = api.getStatus(request("GET", proto::kApiStatus));
    CHECK_EQ(status.status, 200);
    CHECK(status.body.find("\"ipAddress\":\"192.168.1.20\"") != std::string::npos);
    CHECK(status.body.find("\"network\":\"wireless\"") != std::string::npos);
    CHECK(status.body.find("}{") == std::string::npos);     // no concatenated objects
    CHECK(status.body.find("sourceName") == std::string::npos); // no camera-state fields

    // --- /api/profiles: catalog shape, and errors travel as data ------------
    const HttpResponse profiles = api.getProfiles(request("GET", proto::kApiProfiles));
    CHECK_EQ(profiles.status, 200);
    CHECK(profiles.body.find("\"profiles\":[") != std::string::npos);
    CHECK(profiles.body.find("\"name\":\"hd\"") != std::string::npos);
    CHECK(profiles.body.find("\"name\":\"low\"") != std::string::npos);
    CHECK(profiles.body.find("\"state\":\"error\"") != std::string::npos);
    CHECK(profiles.body.find("\"lastError\":\"simulated branch failure\"") != std::string::npos);

    // --- /api/profiles/{name}: known -> 200, unknown -> 404 -----------------
    const HttpResponse hd = api.getProfile(request("GET", "/api/profiles/hd"));
    CHECK_EQ(hd.status, 200);
    CHECK(hd.body.find("\"state\":\"off\"") != std::string::npos);
    CHECK(hd.body.find("\"url\":\"rtsp://127.0.0.1:8554/hd\"") != std::string::npos);

    CHECK_EQ(api.getProfile(request("GET", "/api/profiles/ghost")).status, 404);

    // --- on/off: idempotent, reachable state, unknown name rejected ---------
    const HttpResponse onFirst =
        api.onProfile(request("POST", proto::profileActionPath("hd", proto::kActionOn)));
    CHECK_EQ(onFirst.status, 200);
    CHECK(control.enabled("hd"));
    CHECK(onFirst.body.find("\"state\":\"ready\"") != std::string::npos);

    const HttpResponse onAgain =
        api.onProfile(request("POST", proto::profileActionPath("hd", proto::kActionOn)));
    CHECK_EQ(onAgain.status, 200); // idempotent: same answer, no change
    CHECK(control.enabled("hd"));

    const HttpResponse off =
        api.offProfile(request("POST", proto::profileActionPath("hd", proto::kActionOff)));
    CHECK_EQ(off.status, 200);
    CHECK(!control.enabled("hd"));
    CHECK(off.body.find("\"state\":\"off\"") != std::string::npos);

    CHECK_EQ(api.onProfile(request("POST", proto::profileActionPath("ghost", proto::kActionOn))).status,
             404);
    CHECK(!control.enabled("ghost"));
    CHECK_EQ(api.offProfile(request("POST", proto::profileActionPath("ghost", proto::kActionOff))).status,
             404);

    TEST_RETURN();
}
