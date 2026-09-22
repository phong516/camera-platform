#include "video/VideoPipeline.hpp"
#include "video/TestVideoSource.hpp"

int main(int argc, char *argv[])
{
    Application app;
    if (!app.initialize(argc, argv))
        return -1;
    app.run();
    app.stop();
    return 0;
}
