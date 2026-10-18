#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <glib.h> // GMainLoop, GSource, GMainContext

#include "model/Profile.hpp"
#include "model/SystemStatus.hpp"
#include "video/VideoPipeline.hpp"
#include "web/CameraControl.hpp"

/// Top-level coordinator: owns the capture pipeline and the control-plane
/// state, and is the only place that knows how they fit together.
///
/// Phase 1 (now): preview pipeline (TestVideoSource -> AutoVideoSink) plus
///   the profile catalog served AS DATA - setProfileEnabled() records desired
///   state, no valve branches exist yet to apply it to.
/// Phase 2: the pipeline becomes tee + one valve-gated branch per profile;
///   the desired record becomes the live valve state, profiles() reports it.
/// Phase 3: constructs ApiController with *this, registers routes on a
///   WebServer bound to :8080 (proto::kControlPort).
///
/// Contract for Application.cpp:
///   - initialize(): gst_init first, then TestVideoSource (Phase 9: the real
///     camera source) into pipeline().setVideoSource(), setupPipeline(),
///     signal sources on m_context, attachBusWatch() on m_context. No
///     WebServer/ApiController yet (Phase 3).
///   - run(): play(), g_main_loop_run(m_mainLoop) until requestStop(), then
///     stop(). gst_deinit() belongs to main.cpp, after ~Application.
///   - stop(): pipeline stop, GSource cleanup, main loop unref; idempotent.
///   - CameraControl (web thread once Phase 3 connects):
///       systemStatus()/profiles(): take m_stateMutex, copy out, release.
///         Never hold it across a blocking GStreamer call (set_state/get_state).
///       setProfileEnabled(): idempotent map update under m_stateMutex;
///         unknown name -> false, no change. Until Phase 2 this is the whole
///         effect (desired state only) - that is the documented Phase 1
///         behaviour, not a gap.
///   - profiles(): build from the static catalog below, merging desired state:
///       state = map ? (on ? proto::kStateReady : proto::kStateOff) : kStateOff
///       url from proto::rtspUrlFor(proto::kRtspPort, "/" + name); url must
///       exist in Phase 1 even though the media plane does not - the client
///       contract (ARCHITECTURE section 6) makes it unconditional.
///   - The profile catalog itself is data, not code (ARCHITECTURE section 4):
///     a private static kDefaultCatalog seeded from its table (hd/sd/low/thumb).
///
/// THREADING (exactly 2 threads from Phase 3: main + WebServer::run()):
///   - main thread: run() -> g_main_loop_run(); signal + bus sources fire here
///   - web thread: WebServer::run() -> ApiController -> CameraControl methods
///   m_profileEnabled and m_systemStatus are the only shared state; every
///   access is under m_stateMutex. requestStop() is the only cross-thread
///   entry point outside that (g_main_loop_quit is documented thread-safe).
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
    VideoPipeline m_pipeline;

    /// The catalog is data, not code (see contract block). Defined once in
    /// Application.cpp, seeded from ARCHITECTURE section 4's table:
    /// hd 1920x1080@30 4000k / sd 1280x720@30 2000k / low 854x480@30 800k /
    /// thumb 640x360@15 300k.
    static const std::vector<ProfileInfo> kDefaultCatalog;

    std::map<std::string, bool> m_profileEnabled; // name -> desired valve state (default off)
    SystemStatus m_systemStatus;
    mutable std::mutex m_stateMutex; // guards m_profileEnabled + m_systemStatus

    // --- GLib main-loop pieces (see the contract block above) ---
    bool installSignalHandlers();
    void requestStop(); // idempotent; safe from any thread

    bool attachPipelineEvents(); // PUSH: VideoPipeline raises plain-C++ events
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
