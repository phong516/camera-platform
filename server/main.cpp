#include <gst/gst.h>
#include <thread>

#include "ApiController.hpp"
#include "Application.hpp"
#include "Protocol.hpp"
#include "WebServer.hpp"

/// Headless entry point. The server never creates a QApplication, never opens a
/// window, and never links Qt - it must run under systemd on a board with no
/// display attached.
int main(int argc, char *argv[])
{
    Application app;
    app.initialize(argc, argv);

    WebServer server;

    ApiController api(app);
    api.registerRoutes(server);

    std::thread serverThread(&WebServer::start, &server, proto::kControlPort);
    app.run();
    server.stop();
    if (serverThread.joinable())
    {
        serverThread.join();
    }
    return 0;
}
