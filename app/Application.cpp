#include "app/Application.hpp"
#include "video/TestVideoSource.hpp"
#include <iostream>
#include <glib-unix.h>

Application::~Application()
{
    stop();
    // NOTE: do NOT call gst_deinit() here. This destructor body runs before the
    // member destructors, so ~VideoPipeline() (which unrefs GstElements) would
    // touch Gst after deinit. main() calls gst_deinit() once this object is gone.

    // GLib is independent of Gst: safe to release here. These are plain glib
    // objects, not GstObjects, so they survive past gst_deinit just fine.
    // Signal sources attached to m_context are NOT freed by unref'ing the
    // context: destroy them explicitly, then drop our creation ref.
    for (GSource **source : {&m_sigintSource, &m_sigtermSource})
    {
        if (*source)
        {
            g_source_destroy(*source);
            g_source_unref(*source);
            *source = nullptr;
        }
    }
    if (m_context)
    {
        g_main_context_unref(m_context);
        m_context = nullptr;
    }
}

bool Application::initialize(int argc, char **argv)
{
    gst_init(&argc, &argv);
    // m_context must exist BEFORE installSignalHandlers(): the signal sources
    // are attached to it, and a null context silently means "no handler".
    m_context = g_main_context_new();
    m_mainLoop = g_main_loop_new(m_context, FALSE);
    std::unique_ptr<VideoSource> videoSource = std::make_unique<TestVideoSource>();
    if (!installSignalHandlers())
    {
        std::cerr << "Signal handlers not installed: Ctrl+C will use the default (kill) action." << std::endl;
    }
    if (!m_pipeline.setVideoSource(std::move(videoSource)))
        return false;
    if (!m_pipeline.setupPipeline())
        return false;
    return true;
}

void Application::run()
{
    m_running = true;
    m_pipeline.playPipeline();
    while (m_running)
    {
        // The signal sources live on m_context, so somebody has to iterate it:
        // otherwise SIGINT only lands in the pipe and onUnixSignal never runs.
        // Drain what is pending without blocking, then go back to sleeping.
        while (g_main_context_iteration(m_context, FALSE))
        {
        }
        m_pipeline.pollBus();
        if (!m_pipeline.lastError().empty())
        {
            std::cerr << "Pipeline message: '" << m_pipeline.lastError() << "' --> stopping application." << std::endl;
            m_running = false;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    stop();
}

void Application::stop()
{
    m_running = false; // re-arming the loop here would undo the quit we just handled
    m_pipeline.stopPipeline();
    if (m_mainLoop)
    {
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
    }
}

CameraState Application::cameraState() const
{
    return m_cameraState;
}

SystemStatus Application::systemStatus() const
{
    return m_systemStatus;
}

bool Application::startCamera()
{
    return false;
}
bool Application::stopCamera()
{
    return false;
}
bool Application::setResolution(std::uint32_t width, std::uint32_t height)
{
    if (width <= 0 || height <= 0)
    {
        return false;
    }
    std::lock_guard lock(m_stateMutex);
    m_cameraState.width = width;
    m_cameraState.height = height;
    return true;
}
bool Application::setFrameRate(std::uint32_t fpsNum, std::uint32_t fpsDenom)
{
    if (fpsNum <= 0 || fpsDenom <= 0)
    {
        return false;
    }
    std::lock_guard lock(m_stateMutex);
    m_cameraState.fpsNum = fpsNum;
    m_cameraState.fpsDenom = fpsDenom;
    return false;
}
bool Application::installSignalHandlers()
{
    if (!m_context)
    {
        return false;
    }
    // g_unix_signal_add() hardcodes the DEFAULT GMainContext, which nothing in
    // this app iterates: every source here lives on m_context. Worse, it
    // installs the sigaction for SIGINT immediately, so Ctrl+C stops killing
    // the process the moment it is called, while the callback that would stop
    // us properly never fires. Build the source by hand and attach it to our
    // own context instead.
    //
    // NOTE: g_source_remove(id) does NOT work for a non-default context (it
    // only looks in the default one). Hold the GSource* and destroy it.
    auto attachSignalSource = [this](int signum) -> GSource *
    {
        GSource *source = g_unix_signal_source_new(signum);
        if (!source)
        {
            return nullptr;
        }
        g_source_set_callback(source, &Application::onUnixSignal, this, nullptr);
        g_source_attach(source, m_context);
        return source; // creation ref stays ours; the attachment keeps it alive
    };
    m_sigintSource = attachSignalSource(SIGINT);
    m_sigtermSource = attachSignalSource(SIGTERM);
    return m_sigintSource != nullptr && m_sigtermSource != nullptr;
}
bool Application::attachPipelineEvents()
{
    // assume pipeline is setup
    if (!m_context)
    {
        return false;
    }
    if (!m_pipeline.attachBusWatch(m_context, [this](PipelineEvent event, const std::string &message)
        {
            onPipelineEvent(event, message);
        }))
    {
        return false;
    }
    return true;
}

void Application::onPipelineEvent(PipelineEvent event, const std::string &message)
{

}

gboolean Application::onUnixSignal(gpointer userData)
{
    auto app = static_cast<Application *>(userData);
    app->m_running = false;
    return G_SOURCE_CONTINUE;
}
