cmake_minimum_required(VERSION 3.16)
project(camera-platform VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emit build/compile_commands.json: what the VS Code C/C++ extension and clangd
# use for exact include paths, defines and flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Module path for the GStreamer finders.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(GStreamer REQUIRED)
# RTSP: the media plane. Used by server/ (Phase 2+). Failing here is fatal
# because the server has no other transport.
find_package(GStreamerRtspServer REQUIRED)

# --- Layout -------------------------------------------------------------------
#
#   common/      header-only value types shared by server and clients
#   model/       CameraState / SystemStatus  (no Qt, no Gst)
#   web/         HTTP server + message parsing (POSIX sockets)
#   video/       GStreamer pipeline + video sources
#   server/      headless streaming server            -> camera-server
#   client-qt/   Qt/QML client                        -> camera-client-qt
#   tools/       CLI testers
#
# The server never links Qt. The client never links GStreamer directly.

# --- Qt/QML: only the client needs it (README Phase 5) ------------------------
# find_package() has to run at the top level because Qt6's imported targets AND
# its QT_CMAKE_EXPORT_NAMESPACE helper variable are directory scoped, and
# client-qt/ and its qml/ subdirectory both need them.
#
#   AUTO (default): build the Qt client when Qt6 is found
#   ON:             require Qt6, fail the configure if missing
#   OFF:            never look for Qt; the server still builds
set(CAMERA_PLATFORM_WITH_QT "AUTO" CACHE STRING "Build the Qt/QML client (ON, OFF, AUTO)")
set_property(CACHE CAMERA_PLATFORM_WITH_QT PROPERTY STRINGS ON OFF AUTO)

set(CAMERA_PLATFORM_QT_ENABLED OFF)

if(NOT CAMERA_PLATFORM_WITH_QT STREQUAL "OFF")
    # An earlier configure can cache Qt6_DIR as NOTFOUND (one run before Qt was
    # installed, or a version constraint that did not match). CMake then never
    # searches again and AUTO silently degrades to a server-only build.
    if(DEFINED CACHE{Qt6_DIR} AND NOT Qt6_DIR)
        unset(Qt6_DIR CACHE)
    endif()

    find_package(Qt6 6.10.2 QUIET COMPONENTS
        Core
        Gui
        Widgets
        Qml
        Quick
        QuickControls2
        QuickLayouts
        Network
    )

    if(Qt6_FOUND)
        set(CAMERA_PLATFORM_QT_ENABLED ON)

        # Qt's QML plugin targets (Qt6::qtquick2plugin, ...) live in separate
        # packages under Qt6Qml/QmlPlugins and are NOT pulled in by the
        # COMPONENTS list above. Loading the umbrella QmlPlugins component
        # resolves most of them; without it qt_import_qml_plugins() emits one
        # "will not be linked" warning per plugin.
        #
        # A few style plugins (qtquickcontrols2{basic,fusion,...}styleplugin)
        # still warn. They are cosmetic: the client was verified to load its QML
        # module and run cleanly with QT_QPA_PLATFORM=offscreen and no errors.
        find_package(Qt6 QUIET COMPONENTS QmlPlugins)
        message(STATUS "Qt ${Qt6_VERSION} found - building the QML client (client-qt/)")
    elseif(CAMERA_PLATFORM_WITH_QT STREQUAL "ON")
        message(FATAL_ERROR
            "CAMERA_PLATFORM_WITH_QT=ON but Qt6 >= 6.10.2 was not found.\n"
            "Point CMake at your Qt installation with "
            "-DCMAKE_PREFIX_PATH=<qt-prefix>")
    else()
        message(STATUS "Qt6 not found - building the server only. Install "
                       "qt6-base-dev qt6-declarative-dev, or pass "
                       "-DCMAKE_PREFIX_PATH=<qt-prefix>")
    endif()
endif()

option(CAMERA_PLATFORM_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(common)
add_subdirectory(model)
add_subdirectory(web)
add_subdirectory(video)

add_subdirectory(server)
add_subdirectory(client-qt)

add_subdirectory(tools)

if(CAMERA_PLATFORM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
