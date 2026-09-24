#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include <glib.h> // GMainLoop, g_unix_signal_add sources, guint

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
///   - run(): install the signal sources (g_unix_signal_add), attach ONE bus
///     mechanism (attachPipelineEvents() for push, or attachBusPoll() to keep
///     using VideoPipeline::pollBus()), start playback, then g_main_loop_run()
///     until a source calls requestStop(). Stop the web server before returning.
///   - stop(): pipeline pause/cleanup + webServer.stop(); idempotent.
///   - The destructor must not touch Gst after gst_deinit(); call gst_deinit()
///     at the very end of run(), after the web thread is joined.
///   - startCamera()/stopCamera()/setResolution()/setFrameRate() must keep
///     m_cameraState in sync and return false (never abort) on failure.
///
/// THREADING (exactly 2 threads you own: main + WebServer::run()):
///   - main thread: run() -> g_main_loop_run(); the signal and bus sources fire
///     on this thread's default GMainContext, plus the stats refresh
///   - web thread:  WebServer::run() -> ApiController -> cameraState()/startCamera()
///   m_cameraState and m_systemStatus are therefore shared. Read and write them
///   only while holding m_stateMutex (cameraState(), systemStatus(), startCamera(),
///   stopCamera(), setResolution(), setFrameRate(), onPipelineEvent(), and the
///   stats refresh):
///       std::lock_guard<std::mutex> lock(m_stateMutex);
///   Never hold m_stateMutex across a blocking GStreamer call (set_state,
///   get_state) — take the lock, copy, release, then call Gst.
///   requestStop() is the ONLY method safe to call from another thread:
///   g_main_loop_quit() is documented thread-safe.
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

    // --- GLib main-loop sources (see the contract block above) ---
    bool installSignalHandlers();
    void requestStop(); // idempotent; safe from any thread

    // Pick ONE in run(). Neither lets this class see GstBus or GstMessage:
    // the bus belongs to VideoPipeline (README design principle, lines 110-119).
    bool attachPipelineEvents(); // PUSH: VideoPipeline raises plain-C++ events
    bool attachBusPoll();        // PULL: g_timeout_add -> m_pipeline.pollBus()

    void onPipelineEvent(PipelineEvent event, const std::string &message);

    static gboolean onUnixSignal(gpointer userData);
    static gboolean onBusPoll(gpointer userData);

    GMainLoop *m_mainLoop{nullptr};
    guint m_sigintSourceId{0};
    guint m_sigtermSourceId{0};
    guint m_busPollId{0};

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;
};
