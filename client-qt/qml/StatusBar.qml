import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: layout.implicitHeight * 1.25
    color: "lightgray"
    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 10
        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: "green"
            Layout.alignment: Qt.AlignVCenter;
        }
        Label {
            id: action
            text: "Streaming"
        }
        Label {
            id: resolution
            text: "1280x720"
        }
        Label {
            id: fps
            text: "30 fps"
        }
        Label {
            id: bitrate
            text: "12.4 Mbps"
        }
        Label {
            id: latency
            text: "48 ms"
        }
        Item {
            Layout.fillWidth: true
        }
        Button {
            id: setting
            text: "Settings"
            Layout.rightMargin: 5
        }
    }
}
