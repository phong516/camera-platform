# HTTP/1.1 server and message parsing, built on POSIX sockets.
#
# ApiController.cpp and WebServer.cpp are empty stubs; WebServer.cpp becomes the
# server's control plane in Phase 3. It is compiled here so header-only code and
# future definitions land in one library.
add_library(web_lib
    ApiController.cpp
    WebServer.cpp
)

# Headers carry root-relative includes (model/JsonUtil.hpp, http message
# conventions), so the web target puts the repo root on its path explicitly.
target_include_directories(web_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(web_lib PUBLIC
    model_lib
    common
)
