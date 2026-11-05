#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <glib.h> // GMainLoop, GSource, GMainContext

#include "model/Profile.hpp"
#include "model/SystemStatus.hpp"
#include "video/VideoPipeline.hpp" // PipelineEvent / PipelineEventCallback / PipelineOutcome
#include "web/CameraControl.hpp"

#include "ProfileHost.hpp"
#include "RtspProfileServer.hpp"

/// Top-level coordinator: owns the capture pipeline and the control-plane
/// state, and is the only place that knows how they fit together.
///
/// Phase 1: preview pipeline (TestVideoSource -> AutoVideoSink) with the
///   profile catalog served AS DATA - superseded.
/// Phase 2 (now): the pipeline adapter is ProfileHost (tee + one valve-gated
///   branch per profile); setProfileEnabled() flips the live valve, profiles()
///   reports the off/ready/live state maintained by the host; RtspProfileServer
///   serves the branches on their mounts and publishes ready <-> live as RTSP
///   media is prepared and unprepared.
/// Phase 3: constructs ApiController with *this, registers routes on a
///   WebServer bound to :8080 (proto::kControlPort).
///
/// Contract for Application.cpp:
///   - initialize(): gst_init first, then TestVideoSource (Phase 9: the real
///     camera source) into host().setVideoSource(), setupPipeline(), then
///     m_rtsp = std::make_unique<RtspProfileServer>(m_host) + attach on
///     m_context (NOT in the constructor: the mounts bind appsinks that only
///     exist after setup, and Gst objects need gst_init), signal sources on
///     m_context, attachBusWatch() on m_context. No WebServer/ApiController
///     yet (Phase 3).
///   - run(): host().play(), g_main_loop_run(m_mainLoop) until requestStop(),
///     then stop(). gst_deinit() belongs to main.cpp, after ~Application.
///   - stop(): m_rtsp->detach() first (it references borrowed appsinks and
///     the context), then host stop, GSource cleanup, main loop unref;
///     idempotent.
///   - CameraControl (web thread once Phase 3 connects):
///       systemStatus()/profiles(): take m_stateMutex, copy out, release.
///         Never hold it across a blocking GStreamer call (set_state/get_state).
///       setProfileEnabled(): delegates to host().setProfileEnabled() (the
///         valve is the single source of truth for enabled/disabled, and the
///         reported state follows it); unknown name -> false, no change.
///   - profiles(): host().profiles() already carries catalog values
///       (geometry, url) plus the live profile state: setProfileEnabled()
///       drives off/ready, and RtspProfileServer moves ready <-> live from the
///       media prepared/unprepared events. url must exist even though no RTSP
///       client is connected yet - the client contract (ARCHITECTURE section
///       6) makes it unconditional.
///   - The profile catalog itself is data, not code (ARCHITECTURE section 4):
///     a private static kDefaultCatalog seeded from its table (hd/sd/low/thumb).
///
/// THREADING (exactly 2 threads from Phase 3: main + WebServer::run()):
///   - main thread: run() -> g_main_loop_run(); signal + bus + rtsp sources
///     fire here
///   - web thread: WebServer::run() -> ApiController -> CameraControl methods
///   Shared state: m_systemStatus and m_host's profile state (valve + reported
///   state; setProfileEnabled/profileEnabled/setProfileState are not thread-safe
///   on ProfileHost itself). Every CameraControl call into m_host runs under
///   m_stateMutex, which is never held across play()/stop() (blocking set_state).
///   RtspProfileServer also publishes ready/live from media callbacks on the
///   main thread; once Phase 3 reads profiles from the web thread, that write
///   must go through the same lock (or be marshalled onto m_context). Lifecycle
///   and the main loop are main-thread only. requestStop() is the only
///   cross-thread entry point outside that (g_main_loop_quit is thread-safe).
class Application : public CameraControl
{
public:
    Application();
    ~Application();

    bool initialize(int argc, char **argv);
    void run();
    void stop();

    // CameraControl
    SystemStatus systemStatus() const override;
    std::vector<ProfileInfo> profiles() const override;
    bool setProfileEnabled(const std::string &name, bool enabled) override;

private:
    /// The catalog is data, not code (see contract block). Defined once in
    /// Application.cpp, seeded from ARCHITECTURE section 4's table:
    /// hd 1920x1080@30 4000k / sd 1280x720@30 2000k / low 854x480@30 800k /
    /// thumb 640x360@15 300k.
    static const std::vector<ProfileInfo> kDefaultCatalog;

    /// The pipeline adapter at the seam VideoPipeline used to fill (see the
    /// class comment). m_rtsp is unique_ptr and is CREATED IN initialize(),
    /// after setupPipeline(): RtspProfileServer's constructor builds its
    /// mounts and binds the borrowed appsinks, so gst_init + branch setup
    /// must exist by then. Declared after m_host - it holds a host reference,
    /// so destruction is m_rtsp -> m_host.
    ProfileHost m_host;
    std::unique_ptr<RtspProfileServer> m_rtsp;

    SystemStatus m_systemStatus;
    mutable std::mutex m_stateMutex; // guards m_systemStatus + CameraControl calls into m_host

    // --- GLib main-loop pieces (see the contract block above) ---
    bool installSignalHandlers();
    void requestStop(); // idempotent; safe from any thread

    bool attachPipelineEvents(); // PUSH: ProfileHost raises plain-C++ events
    void onPipelineEvent(PipelineEvent event, const std::string &message);

    static gboolean onUnixSignal(gpointer userData);

    GMainLoop *m_mainLoop{nullptr};
    // GSource*, not source ids: g_source_remove() cannot reach a source on a
    // non-default context, so we destroy these by pointer.
    GSource *m_sigintSource{nullptr};
    GSource *m_sigtermSource{nullptr};

    GMainContext *m_context{nullptr};

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;
};
