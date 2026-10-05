#include "app/Application.hpp"
#include "video/TestVideoSource.hpp"
#include <iostream>
#include <csignal>

static volatile std::sig_atomic_t g_running = 1;

static void signalHandler(int signal)
{
    g_running = 0;
}

Application::~Application()
{
    stop();
    // NOTE: do NOT call gst_deinit() here. This destructor body runs before the
    // member destructors, so ~VideoPipeline() (which unrefs GstElements) would
    // touch Gst after deinit. main() calls gst_deinit() once this object is gone.

    // GLib is independent of Gst: safe to release here. These are plain glib
    // objects, not GstObjects, so they survive past gst_deinit just fine.
    if (m_mainLoop)
    {
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
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
    std::signal(SIGINT, &signalHandler);
    std::signal(SIGTERM, &signalHandler);
    std::unique_ptr<VideoSource> videoSource = std::make_unique<TestVideoSource>();
    m_context = g_main_context_new();
    m_mainLoop = g_main_loop_new(m_context, FALSE);
    if (!m_pipeline.setVideoSource(std::move(videoSource)))
        return false;
    if (!m_pipeline.setupPipeline())
        return false;
    return true;
}

void Application::run()
{
    g_running = 1;
    m_pipeline.playPipeline();
    while (g_running)
    {
        m_pipeline.pollBus();
        if (!m_pipeline.lastError().empty())
        {
            std::cerr << "Pipeline message: '" << m_pipeline.lastError() << "' --> stopping application." << std::endl;
            g_running = 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    stop();
}

void Application::stop()
{
    m_pipeline.stopPipeline();
    g_running = 1;
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
