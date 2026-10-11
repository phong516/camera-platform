# Locate gstreamer-rtsp-server-1.0, the media plane for server/.
#
# Mirrors FindGStreamer.cmake: pkg-config for the flags, then the full link
# closure so consumers get gst-rtsp-server, gst-rtsp-server's gst/glib
# dependencies, and gio without listing them by hand.

find_package(PkgConfig REQUIRED)

pkg_check_modules(PKG_GSTREAMER_RTSP_SERVER REQUIRED gstreamer-rtsp-server-1.0)

find_path(GSTREAMER_RTSP_SERVER_INCLUDE_DIR
    NAMES gst/rtsp-server/rtsp-server.h
    HINTS ${PKG_GSTREAMER_RTSP_SERVER_INCLUDE_DIRS}
)

find_library(GSTREAMER_RTSP_SERVER_LIBRARIES
    NAMES gstrtspserver-1.0
    HINTS ${PKG_GSTREAMER_RTSP_SERVER_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GStreamerRtspServer DEFAULT_MSG
    GSTREAMER_RTSP_SERVER_LIBRARIES GSTREAMER_RTSP_SERVER_INCLUDE_DIR)

if(GStreamerRtspServer_FOUND)
    set(GSTREAMER_RTSP_SERVER_INCLUDE_DIRS
        ${PKG_GSTREAMER_RTSP_SERVER_INCLUDE_DIRS}
    )
    set(GSTREAMER_RTSP_SERVER_ALL_LIBRARIES
        ${PKG_GSTREAMER_RTSP_SERVER_LINK_LIBRARIES}
    )
    message(STATUS "gstreamer-rtsp-server ${PKG_GSTREAMER_RTSP_SERVER_VERSION} found")
endif()
