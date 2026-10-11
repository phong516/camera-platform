import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 40
    width: parent.width
    color: "lightgray"
    
    RowLayout {
        anchors.fill: parent
        Label {
            id: action
        }
        Label {
            id: resolution
        }
        Label {
            id: fps
        }
        Label {
            id: bitrate
        }
        Label {
            id: latency
        }
        Button {
            id: setting
        }
    }
}