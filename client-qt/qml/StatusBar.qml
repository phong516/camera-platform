import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: layout.implicitHeight * 1.25
    color: "lightgray"

    property string statusText: "off"
    property string resolution: "0x0"
    property int fps: 0
    property bool streaming: false
    property double bitrateKbps: 0.0
    property double cpuPercent: 0.0
    property string lastError: ""

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 10
        Rectangle {
            implicitWidth: 10
            implicitHeight: 10
            radius: 5
            color: root.streaming ? "green" : "red"
            Layout.alignment: Qt.AlignVCenter;
        }
        Label {
            id: action
            text: root.statusText
        }
        Label {
            id: resolution
            text: root.resolution
        }
        Label {
            id: fps
            text: root.fps + " fps"
        }
        Label {
            id: bitrate
            text: root.bitrateKbps + " Mbps"
        }
        Label {
            id: latency
            text: "0 ms"
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
