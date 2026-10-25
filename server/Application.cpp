#include "server/Application.hpp"
#include "video/TestVideoSource.hpp"
#include <iostream>
#include <glib-unix.h>

const std::vector<ProfileInfo> Application::kDefaultCatalog = {
    {.name = "hd", .width = 1920, .height = 1080, .fps = 30, .bitrateKbps = 4000, .url = proto::rtspUrlFor(proto::kRtspPort, "hd")},
    {.name = "sd", .width = 1280, .height = 720, .fps = 30, .bitrateKbps = 2000, .url = proto::rtspUrlFor(proto::kRtspPort, "sd")},
    {.name = "low", .width = 854, .height = 480, .fps = 30, .bitrateKbps = 800, .url = proto::rtspUrlFor(proto::kRtspPort, "low")},
    {.name = "thumb", .width = 640, .height = 360, .fps = 15, .bitrateKbps = 300, .url = proto::rtspUrlFor(proto::kRtspPort, "thumb")},
};

Application::Application()
{
    for (const ProfileInfo &profile : kDefaultCatalog)
    {
        m_profileEnabled[profile.name] = false;
    }
}

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
    if (!m_pipeline.setVideoSource(std::move(videoSource)).ok)
        return false;
    if (!m_pipeline.setupPipeline().ok)
        return false;
    if (!attachPipelineEvents())
        return false;
    return true;
}

void Application::run()
{
    m_pipeline.play();
    if (m_mainLoop)
    {
        g_main_loop_run(m_mainLoop);
    }
    stop();
}

void Application::stop()
{
    m_pipeline.stop();
    if (m_mainLoop)
    {
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
    }
}

SystemStatus Application::systemStatus() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_systemStatus;
}

std::vector<ProfileInfo> Application::profiles() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    std::vector<ProfileInfo> catalog{kDefaultCatalog};
    for (ProfileInfo &profile : catalog)
    {
        const auto it = m_profileEnabled.find(profile.name);
        const bool on = (it != m_profileEnabled.end() && it->second);
        profile.state = on ? proto::kStateReady : proto::kStateOff;
    }
    return catalog;
}

bool Application::setProfileEnabled(const std::string &name, bool enabled)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (!m_profileEnabled.count(name))
    {
        return false;
    }
    m_profileEnabled[name] = enabled;
    return true;
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

void Application::requestStop()
{
    if (m_mainLoop)
    {
        g_main_loop_quit(m_mainLoop);
    }
}

bool Application::attachPipelineEvents()
{
    // assume pipeline is setup
    if (!m_context)
    {
        return false;
    }
    if (!m_pipeline.attachBusWatch(m_context, [this](PipelineEvent event, const std::string &message)
                                   { onPipelineEvent(event, message); })
             .ok)
    {
        return false;
    }
    return true;
}

void Application::onPipelineEvent(PipelineEvent event, const std::string &message)
{
    switch (event)
    {
    case PipelineEvent::Error:
        std::cerr << "Pipeline error: " << message << std::endl;
        requestStop();
        break;
    case PipelineEvent::EndOfStream:
        std::cout << "End of stream\n";
        requestStop();
        break;
    case PipelineEvent::Warning:
        std::cout << "Pipeline warning: " << message << std::endl;
        break;
    default:
        break;
    }
}

gboolean Application::onUnixSignal(gpointer userData)
{
    std::cout << "Ctrl + C\n";
    auto app = static_cast<Application *>(userData);
    app->requestStop();
    return G_SOURCE_CONTINUE;
}
