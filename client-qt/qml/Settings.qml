import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "lightgray"
    implicitHeight: content.implicitHeight
    property var profiles: []
    property string resolution: ""
    property int fps: 0
    readonly property var selectedProfile: profiles.find(profile => profile.name == profilesList.currentText)
    property bool start: false
    property bool stop: false
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 10
        GridLayout {
            columns: 2
            columnSpacing: 5
            rowSpacing: 5
            Label {
                text: "Source"
            }
            ComboBox {
                id: profilesList
                model: root.profiles.map(profile => profile.name)
                currentIndex: 0
            }
            Label {
                text: "Resolution"
            }
            Label {
                text: root.selectedProfile ? root.selectedProfile.width + "x" + root.selectedProfile.height : ""
            }
            Label {
                text: "FPS"
            }
            Label {
                text: root.selectedProfile ? root.selectedProfile.fps : ""
            }
        }
        RowLayout {
            Button {
                text: "Start"
                onClicked: {
                    root.start = true
                    root.stop = false
                }
            }
            Button {
                text: "Stop"
                onClicked: {
                    root.stop = true
                    root.start = false
                }
            }
        }
    }
}
