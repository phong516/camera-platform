// ===========================================================================
// StatusBar.qml - top strip. Spec: client-qt/UI-PROTOTYPE.md
// Read-only: it never calls backend, it only binds.
//
// TODO (Phase 5)
//   [ ] Add the required property the parent passes in:
//         required property var backend
//   [ ] Replace the hardcoded strings with bindings:
//         action:     backend.statusText
//         resolution: backend.resolution
//         fps:        backend.fps + " fps"
//         bitrate:    backend.bitrateKbps.toFixed(1) + " Mbps"
//         latency:    NO SUCH PROPERTY yet - see note below
//   [ ] Add the status dot: Rectangle { width: 12; height: 12; radius: 6 }
//         idle grey | starting amber | streaming green | paused amber | error red
//         colour via a JS function: color: root.dotColor(backend.statusText)
//   [ ] Wrap in Connections { target: backend } with onCameraChanged and
//       onStatsChanged, so you can see the pushes the backend emits.
//   [ ] latencyMs is NOT in CameraBackend.hpp. Delete the latency Label or add
//       the Q_PROPERTY + a NOTIFY signal to CameraBackend first.
//   [ ] The Settings button should emit a signal (signal settingsClicked())
//       and let Main.qml decide what opens.
// ===========================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 40
    color: "lightgray"
    
    RowLayout {
        anchors.fill: parent
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
        Button {
            id: setting
            text: "Settings"
        }
    }
}
