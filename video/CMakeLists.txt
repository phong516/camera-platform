add_library(video_lib
    CameraVideoSource.cpp
    TestVideoSource.cpp
    VideoPipeline.cpp
)

target_include_directories(video_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GSTREAMER_INCLUDE_DIRS}
)

target_link_libraries(video_lib PUBLIC
    ${GSTREAMER_ALL_LIBRARIES}
)