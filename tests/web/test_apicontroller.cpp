#include <map>
#include <string>
#include <vector>

#include "common/Protocol.hpp"
#include "model/Profile.hpp"
#include "model/SystemStatus.hpp"
#include "tests/TestSupport.hpp"
#include "web/ApiController.hpp"

/// Phase 3 test: the section 6 control-plane surface in common/Protocol.hpp,
/// served by ApiController over the CameraControl seam.
///
/// Two layers are tested:
///   1. the handler methods directly, with hand-built HttpRequests (the test
///      surface promised by web/ApiController.hpp);
///   2. registerRoutes() through WebServer::dispatch(), proving the exact
///      "METHOD /path" keys and the router's 404/405 behaviour.
///
/// NOT LINKABLE YET: web/ApiController.cpp is still empty. Its block in
/// tests/CMakeLists.txt is enabled, so this test turns green as the
/// implementation lands.

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
        hd.url = proto::rtspUrlFor(proto::kRtspPort, "hd");
        hd.state = m_enabled.count("hd") && m_enabled.at("hd")
                       ? proto::kStateReady
                       : proto::kStateOff;

        ProfileInfo low;
        low.name = "low";
        low.width = 854;
        low.height = 480;
        low.fps = 30;
        low.bitrateKbps = 800;
        low.url = proto::rtspUrlFor(proto::kRtspPort, "low");
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
    CHECK_EQ(health.contentType, std::string("application/json"));
    CHECK_EQ(health.body, std::string("{\"ok\":true}"));

    // --- /api/status: exactly the SystemStatus JSON, nothing concatenated ---
    const HttpResponse status = api.getStatus(request("GET", proto::kApiStatus));
    CHECK_EQ(status.status, 200);
    CHECK_EQ(status.body, toJson(control.systemStatus()));
    CHECK(status.body.find("\"network\":\"wireless\"") != std::string::npos);
    CHECK(status.body.find("sourceName") == std::string::npos); // no camera-state fields
    CHECK(status.body.find("}{") == std::string::npos);         // no concatenated objects

    // --- /api/profiles: catalog shape, order, and errors as data ------------
    const std::vector<ProfileInfo> catalog = control.profiles();
    const std::string expectedProfiles =
        std::string("{\"profiles\":[") + toJson(catalog[0]) + "," + toJson(catalog[1]) + "]}";

    const HttpResponse profiles = api.getProfiles(request("GET", proto::kApiProfiles));
    CHECK_EQ(profiles.status, 200);
    CHECK_EQ(profiles.body, expectedProfiles);
    CHECK(profiles.body.find("\"name\":\"hd\"") != std::string::npos);
    CHECK(profiles.body.find("\"name\":\"low\"") != std::string::npos);
    CHECK(profiles.body.find("\"state\":\"error\"") != std::string::npos);
    CHECK(profiles.body.find("\"lastError\":\"simulated branch failure\"") != std::string::npos);

    // --- /api/profiles/{name}: known -> 200, unknown -> 404 -----------------
    const HttpResponse hd = api.getProfile(request("GET", "/api/profiles/hd"));
    CHECK_EQ(hd.status, 200);
    CHECK_EQ(hd.body, toJson(catalog[0]));
    CHECK(hd.body.find("\"state\":\"off\"") != std::string::npos);
    CHECK(hd.body.find("\"url\":\"rtsp://127.0.0.1:8554/hd\"") != std::string::npos);

    const HttpResponse ghost = api.getProfile(request("GET", "/api/profiles/ghost"));
    CHECK_EQ(ghost.status, 404);
    CHECK(ghost.body.find("\"error\"") != std::string::npos);

    // Path parsing is internal: missing and over-long names are not catalog hits.
    CHECK_EQ(api.getProfile(request("GET", "/api/profiles/")).status, 404);
    CHECK_EQ(api.getProfile(request("GET", "/api/profiles/hd/extra")).status, 404);

    // --- on/off: idempotent, state AFTER the change, unknown rejected --------
    const HttpResponse onFirst =
        api.onProfile(request("POST", proto::profileActionPath("hd", proto::kActionOn)));
    CHECK_EQ(onFirst.status, 200);
    CHECK(control.enabled("hd"));
    CHECK_EQ(onFirst.body, toJson(control.profiles()[0]));
    CHECK(onFirst.body.find("\"state\":\"ready\"") != std::string::npos);

    const HttpResponse onAgain =
        api.onProfile(request("POST", proto::profileActionPath("hd", proto::kActionOn)));
    CHECK_EQ(onAgain.status, 200); // idempotent: same answer, no change
    CHECK_EQ(onAgain.body, onFirst.body);
    CHECK(control.enabled("hd"));

    const HttpResponse off =
        api.offProfile(request("POST", proto::profileActionPath("hd", proto::kActionOff)));
    CHECK_EQ(off.status, 200);
    CHECK(!control.enabled("hd"));
    CHECK_EQ(off.body, toJson(control.profiles()[0]));
    CHECK(off.body.find("\"state\":\"off\"") != std::string::npos);

    const HttpResponse offAgain =
        api.offProfile(request("POST", proto::profileActionPath("hd", proto::kActionOff)));
    CHECK_EQ(offAgain.status, 200); // idempotent
    CHECK_EQ(offAgain.body, off.body);

    CHECK_EQ(api.onProfile(request("POST", proto::profileActionPath("ghost", proto::kActionOn))).status,
             404);
    CHECK(!control.enabled("ghost"));
    CHECK_EQ(api.offProfile(request("POST", proto::profileActionPath("ghost", proto::kActionOff))).status,
             404);
    CHECK(!control.enabled("ghost"));

    // --- registerRoutes() wires the exact section 6 keys --------------------
    // Same server instance, no socket: dispatch() is the observable interface
    // of "what got registered".
    WebServer server;
    api.registerRoutes(server);

    const HttpResponse routedHealth = server.dispatch(request("GET", proto::kApiHealth));
    CHECK_EQ(routedHealth.status, 200);
    CHECK_EQ(routedHealth.body, health.body);

    CHECK_EQ(server.dispatch(request("GET", proto::kApiStatus)).body, status.body);
    CHECK_EQ(server.dispatch(request("GET", proto::kApiProfiles)).body, profiles.body);
    CHECK_EQ(server.dispatch(request("GET", "/api/profiles/hd")).body, hd.body);
    CHECK_EQ(server.dispatch(request("GET", "/api/profiles/low")).status, 200);

    // Unknown names were never registered: the router answers 404.
    CHECK_EQ(server.dispatch(request("GET", "/api/profiles/ghost")).status, 404);
    CHECK_EQ(server.dispatch(request("POST", proto::profileActionPath("ghost", proto::kActionOn))).status,
             404);

    // Registered path, wrong method: the router answers 405, not the handler.
    CHECK_EQ(server.dispatch(request("POST", proto::kApiHealth)).status, 405);
    CHECK_EQ(server.dispatch(request("GET", proto::profileActionPath("hd", proto::kActionOn))).status,
             405);

    // Actions are registered per catalog name and reach CameraControl.
    const HttpResponse routedOn =
        server.dispatch(request("POST", proto::profileActionPath("hd", proto::kActionOn)));
    CHECK_EQ(routedOn.status, 200);
    CHECK(control.enabled("hd"));
    CHECK_EQ(routedOn.body, toJson(control.profiles()[0]));

    // A profile in error still accepts the action; the error travels as data.
    const HttpResponse lowOn =
        server.dispatch(request("POST", proto::profileActionPath("low", proto::kActionOn)));
    CHECK_EQ(lowOn.status, 200);
    CHECK(control.enabled("low"));
    CHECK(lowOn.body.find("\"state\":\"error\"") != std::string::npos);

    const HttpResponse routedOff =
        server.dispatch(request("POST", proto::profileActionPath("hd", proto::kActionOff)));
    CHECK_EQ(routedOff.status, 200);
    CHECK(!control.enabled("hd"));

    TEST_RETURN();
}
