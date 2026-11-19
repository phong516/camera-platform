import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "Camera Platform"
    visible: true
    visibility: Window.Maximized
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint | Qt.WindowMinimizeButtonHint
    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        StatusBar {
            statusText: backend.statusText
            resolution: backend.resolution
            fps: backend.fps
            streaming: backend.streaming
            bitrateKbps: backend.bitrateKbps
            cpuPercent: backend.cpuPercent
            lastError: backend.lastError
            Layout.fillWidth: true
        }
        CameraView {
            id: camera
            profile: settings.selectedProfile
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        Settings {
            id: settings
            onStartChanged:
            {
                if (settings.start && settings.selectedProfile)
                {
                    backend.play(settings.selectedProfile)
                    camera.start = true
                }
            }
            profiles: backend.profiles
            Layout.fillWidth: true
        }
    }

    CameraBackend {
        id: backend
    }
}
