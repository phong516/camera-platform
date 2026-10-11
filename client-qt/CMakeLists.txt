# Qt/QML client. Talks to the server over HTTP for control and RTSP for video.
# It must never link GStreamer directly (see docs/ARCHITECTURE-SERVER-CLIENT.md).

if(NOT CAMERA_PLATFORM_QT_ENABLED)
    message(STATUS "client-qt/ skipped (Qt6 not available)")
    return()
endif()

# AUTOMOC/AUTORCC/AUTOUIC and the qt_add_qml_module defaults. Scoped to this
# directory and below: client-qt/ is the only subtree with Qt-flavoured C++.
qt_standard_project_setup(REQUIRES 6.10.2)

# Defines the camera_platform_qml module target and embeds the .qml files as
# resources. Must come before the executable, which links it.
add_subdirectory(qml)

qt_add_executable(camera-client-qt
    main.cpp
)

target_include_directories(camera-client-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}          # for common/, model/
)

target_link_libraries(camera-client-qt PRIVATE
    camera_platform_qml
    # A STATIC QML module does not pull its own objects out of the archive:
    # without the plugin, qml_register_types_CameraPlatform and the embedded
    # resources are dropped and loadFromModule("CameraPlatform", ...) fails
    # with "No module named CameraPlatform found".
    camera_platform_qmlplugin
    Qt6::Widgets          # QApplication/QWidget, used by main.cpp
    Qt6::Network          # HTTP control client (Phase 4)
    common
    model_lib
)
