import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    title: "Camera Platform"
    visible: true
    width: Screen.width
    height: Screen.height
    flags: Qt.Window | 
            Qt.WindowTitleHint |
            Qt.WindowSystemMenuHint |
            Qt.WindowCloseButtonHint |
            Qt.WindowMinimizeButtonHint
    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        StatusBar {
        }
        CameraView {
        }
        Settings {

        }
    }
}