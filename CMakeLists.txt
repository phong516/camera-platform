cmake_minimum_required(VERSION 3.16)
project(camera-platform VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Module path for module GStreamer finder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(GStreamer REQUIRED)

# Tests link against headers and the existing targets only; see tests/CMakeLists.txt.
option(CAMERA_PLATFORM_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(video)
add_subdirectory(app)

if (CAMERA_PLATFORM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
