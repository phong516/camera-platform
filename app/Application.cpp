#include "Application.hpp"
#include "video/TestVideoSource.hpp"
#include "csignal"
#include <iostream>

Application::~Application()
{
    stop();
    if (gst_is_initialized())
    {
        gst_deinit();
    }
}

bool Application::initialize(int argc, char **argv)
{
    gst_init(&argc, &argv);
    std::unique_ptr<VideoSource> videoSource = std::make_unique<TestVideoSource>();
    m_context = g_main_context_new();
    m_mainLoop = g_main_loop_new(context, FALSE);
    if (!m_pipeline.setVideoSource(std::move(videoSource)))
        return false;
    if (!m_pipeline.setupPipeline())
        return false;
    return true;
}

void Application::run()
{
    std::signal(SIGINT, [this](int signal) { m_running = false; });
    std::signal(SIGTERM, [this](int signal) { m_running = false; });
    m_running = true;
    while (m_running)
    {
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
    m_pipeline.stopPipeline();
    m_running = false;
}

bool Application::attachPipelineEvents()
{
    // assume pipeline is setup
    if (!m_context)
    {
        return false;
    }
    if (!m_pipeline->attachBusWatch(m_context, &onPipelineEvent))
    {
        return false;
    }
    return true;
}

void Application::onPipelineEvent(PipelineEvent event, const std::string &message)
{

}
