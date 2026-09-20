find_package(PkgConfig REQUIRED)
pkg_check_modules(PKG_GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(PKG_GSTREAMER_BASE REQUIRED gstreamer-base-1.0)

find_path(GSTREAMER_INCLUDE_DIR gst/gst.h
    HINTS ${PKG_GSTREAMER_INCLUDE_DIRS} ${PKG_GSTREAMER_BASE_INCLUDE_DIRS}
)

find_library(GSTREAMER_LIBRARIES NAMES gstreamer-1.0
    HINTS ${PKG_GSTREAMER_LIBRARY_DIRS} ${PKG_GSTREAMER_BASE_LIBRARY_DIRS}
)

find_library(GSTREAMER_BASE_LIBRARIES NAMES gstbase-1.0
    HINTS ${PKG_GSTREAMER_LIBRARY_DIRS} ${PKG_GSTREAMER_BASE_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GStreamer DEFAULT_MSG
    GSTREAMER_LIBRARIES GSTREAMER_INCLUDE_DIR)

if (GSTREAMER_FOUND)
    set(GSTREAMER_INCLUDE_DIRS
        ${PKG_GSTREAMER_INCLUDE_DIRS}
        ${PKG_GSTREAMER_BASE_INCLUDE_DIRS}
    )
    # Use the full pkg-config link closure (gstreamer, gstbase, gobject, glib, ...)
    # instead of only the two top-level libs: consumers of the static library
    # reference gobject/glib symbols directly (e.g. g_object_set).
    set(GSTREAMER_ALL_LIBRARIES
        ${PKG_GSTREAMER_LINK_LIBRARIES}
        ${PKG_GSTREAMER_BASE_LINK_LIBRARIES}
    )
endif()