import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "Camera Platform"
    visible: true
    // width: Screen.width
    // height: Screen.height
    visibility: Window.Maximized
    flags: Qt.Window | 
            Qt.WindowTitleHint |
            Qt.WindowSystemMenuHint |
            Qt.WindowCloseButtonHint |
            Qt.WindowMinimizeButtonHint
    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        StatusBar {
            Layout.fillWidth: true
        }
        CameraView {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        Settings {
            Layout.fillWidth: true
        }
    }
}
