#include <cstdint>
#include <string>

#include "model/CameraState.hpp"
#include "model/SystemStatus.hpp"
#include "tests/TestSupport.hpp"
#include "web/ApiController.hpp"
#include "web/WebServer.hpp"

/// NOT COMPILED YET: this test needs web/ApiController.cpp.
/// Uncomment the test_apicontroller block in tests/CMakeLists.txt once it exists.

namespace
{
class FakeControl : public CameraControl
{
public:
    CameraState cameraState() const override
    {
        CameraState state;
        state.status = CameraStatus::Streaming;
        state.sourceName = "videotestsrc";
        state.width = 1280;
        state.height = 720;
        return state;
    }

    SystemStatus systemStatus() const override
    {
        SystemStatus status;
        status.network = NetworkState::Wireless;
        status.ipAddress = "192.168.1.20";
        return status;
    }

    bool startCamera() override
    {
        m_streaming = true;
        return true;
    }

    bool stopCamera() override
    {
        m_streaming = false;
        return true;
    }

    bool setResolution(std::uint32_t, std::uint32_t) override { return true; }
    bool setFrameRate(std::uint32_t, std::uint32_t) override { return true; }

    bool streaming() const { return m_streaming; }

private:
    bool m_streaming{false};
};
} // namespace

int main()
{
    FakeControl control;
    ApiController api(control);

    HttpRequest statusRequest;
    statusRequest.method = "GET";
    statusRequest.path = "/api/status";

    const HttpResponse status = api.getStatus(statusRequest);
    CHECK_EQ(status.status, 200);
    CHECK(status.body.find("\"status\":\"streaming\"") != std::string::npos);
    CHECK(status.body.find("\"sourceName\":\"videotestsrc\"") != std::string::npos);
    CHECK(status.body.find("\"ipAddress\":\"192.168.1.20\"") != std::string::npos);

    HttpRequest toggleRequest;
    toggleRequest.method = "POST";
    toggleRequest.path = "/api/camera/start";

    CHECK_EQ(api.startCamera(toggleRequest).status, 200);
    CHECK(control.streaming());

    toggleRequest.path = "/api/camera/stop";
    CHECK_EQ(api.stopCamera(toggleRequest).status, 200);
    CHECK(!control.streaming());

    HttpRequest configRequest;
    configRequest.method = "GET";
    configRequest.path = "/api/config";
    CHECK_EQ(api.getConfig(configRequest).status, 200);

    HttpRequest badConfig;
    badConfig.method = "POST";
    badConfig.path = "/api/config";
    badConfig.body = "{}";
    CHECK_EQ(api.updateConfig(badConfig).status, 400);

    TEST_RETURN();
}
