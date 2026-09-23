#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "model/CameraState.hpp"
#include "model/SystemStatus.hpp"
#include "streaming/CloudStreamer.hpp"
#include "streaming/StreamServer.hpp"
#include "system/NetworkManager.hpp"
#include "video/VideoPipeline.hpp"
#include "web/ApiController.hpp"
#include "web/WebServer.hpp"

/// Top-level coordinator: owns the pipeline and every service, and is the only
/// place that knows how they fit together.
///
/// STATUS: header only. Application.cpp is the implementation task.
///
/// Contract for Application.cpp:
///   - initialize(): gst_init(argc, argv) first, then create the source
///     (TestVideoSource for now, CameraVideoSource in Phase 9), call
///     pipeline().setVideoSource() + setupPipeline(), construct m_apiController
///     with *this, registerRoutes(m_webServer), start the web server on 8080.
///     A failed web-server bind must NOT abort the preview: log and continue.
///   - run(): install SIGINT/SIGTERM handlers that clear m_running, then loop
///     on m_running, calling m_pipeline.pollBus() and sleeping ~100 ms. Exit on
///     bus ERROR/EOS. Stop the web server before returning.
///   - stop(): pipeline pause/cleanup + webServer.stop(); idempotent.
///   - The destructor must not touch Gst after gst_deinit(); call gst_deinit()
///     at the very end of run(), after the web thread is joined.
///   - startCamera()/stopCamera()/setResolution()/setFrameRate() must keep
///     m_cameraState in sync and return false (never abort) on failure.
///
/// THREADING (exactly 2 threads you own: main + WebServer::run()):
///   - main thread: run() -> m_pipeline.pollBus() and the stats refresh
///   - web thread:  WebServer::run() -> ApiController -> cameraState()/startCamera()
///   m_cameraState and m_systemStatus are therefore shared. Read and write them
///   only while holding m_stateMutex (cameraState(), systemStatus(), startCamera(),
///   stopCamera(), setResolution(), setFrameRate(), and the run() stats refresh):
///       std::lock_guard<std::mutex> lock(m_stateMutex);
///   Never hold m_stateMutex across a blocking GStreamer call (set_state,
///   get_state, pollBus) — take the lock, copy, release, then call Gst.
///   Phase 4/5 add threads (RTSP main loop, Qt GUI thread); this mutex stays the
///   single place application state is synchronized.
class Application : public CameraControl
{
public:
    Application() = default;
    ~Application();

    bool initialize(int argc, char **argv);
    void run();
    void stop();

    // CameraControl
    CameraState cameraState() const override;
    SystemStatus systemStatus() const override;

    bool startCamera() override;
    bool stopCamera() override;
    bool setResolution(std::uint32_t width, std::uint32_t height) override;
    bool setFrameRate(std::uint32_t fpsNum, std::uint32_t fpsDenom) override;

    VideoPipeline &pipeline();
    WebServer &webServer();
    NetworkManager &networkManager();
    StreamServer &streamServer();
    CloudStreamer &cloudStreamer();

private:
    VideoPipeline m_pipeline;
    WebServer m_webServer;
    std::unique_ptr<ApiController> m_apiController; // needs *this, built in initialize()
    NetworkManager m_networkManager;
    StreamServer m_streamServer;
    CloudStreamer m_cloudStreamer;
    CameraState m_cameraState;
    SystemStatus m_systemStatus;
    mutable std::mutex m_stateMutex; // guards m_cameraState + m_systemStatus
    std::atomic<bool> m_running{false};

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;
};
