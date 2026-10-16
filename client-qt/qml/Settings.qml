import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
   id: root
   color: "lightgray"
   implicitHeight: content.implicitHeight
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
            model: ["videotestsrc", "/dev/video0"]
            currentIndex: 0
        }
        Label {
            text: "Resolution"
        }
        ComboBox {
            model: ["140p", "240p", "360p", "720p", "1040p"]
            currentIndex: 0
        }
        Label {
            text: "FPS"
        }
        ComboBox {
            model: ["30", "60"]
            currentIndex: 0
        }
    }
    RowLayout {
        Button {
            text: "Start"
        }
        Button {
            text: "Stop"
        }
    }
   } 
}
