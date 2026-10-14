# The QML module. STATIC, so the .qml files and their qmldir are compiled into
# the client binary as Qt resources - no loose .qml files on the target device.
#
# CameraBackend.hpp/.cpp live in this directory rather than client-qt/src/: the
# only Qt C++ the client has is this QML bridge.

# Write .qmlls.ini next to these files so the VS Code Qt extension points qmlls
# at this project's QML module.
set(QT_QML_GENERATE_QMLLS_INI ON)

qt_add_qml_module(camera_platform_qml
    URI CameraPlatform
    VERSION 1.0
    STATIC
    # The module's target path is CameraPlatform, so its output directory must
    # end in the same name or qmllint/qmlls cannot resolve the module.
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/CameraPlatform
    QML_FILES
        Main.qml
        CameraView.qml
        StatusBar.qml
        Settings.qml
    SOURCES
        CameraBackend.cpp
)

target_include_directories(camera_platform_qml PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(camera_platform_qml PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
    common
    model_lib
)
