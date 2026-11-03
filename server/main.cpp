#include <gst/gst.h>

#include "Application.hpp"

/// Headless entry point. The server never creates a QApplication, never opens a
/// window, and never links Qt - it must run under systemd on a board with no
/// display attached.
int main(int argc, char *argv[])
{
    Application app;
    app.initialize(argc, argv);
    app.setProfileEnabled("hd", true);
    app.run();
    return 0;
}
