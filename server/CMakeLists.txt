# Headless streaming server. Links GStreamer, never Qt.
#
# Phase 1: runs the existing single-sink VideoPipeline to autovideosink so the
# target exists and builds. Phase 2 replaces it with ProfileHost (one pipeline,
# tee + one valve-gated branch per profile) and RtspProfileServer.

set(CAMERA_SERVER_SOURCES
    main.cpp
    Application.cpp
    ProfileBin.cpp
    ProfileHost.cpp
)

add_library(camera_server_lib STATIC
    Application.cpp
    ProfileBin.cpp
    ProfileHost.cpp
)

# Repo-root-relative includes are the project convention (e.g.
# "server/Application.hpp", "video/VideoPipeline.hpp"), so every target that
# includes across directories needs the root on its path.

target_include_directories(camera_server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(camera_server_lib PUBLIC
    video_lib
    web_lib
    model_lib
    common
)

add_executable(camera-server
    main.cpp
)

target_link_libraries(camera-server PRIVATE
    camera_server_lib
)
