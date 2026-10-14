// ===========================================================================
// Main.qml - window shell. Spec: client-qt/UI-PROTOTYPE.md
//
// TODO (Phase 5)
//   [x] Uncomment CameraView{} and Settings{} below.
//       CameraView needs Layout.fillHeight: true (it is the stretchy middle row).
//   [x] Uncomment the same two files in client-qt/qml/CMakeLists.txt - a .qml
//       that is not in QML_FILES is never compiled into the module.
//   [ ] Add ErrorBanner as the LAST child of the ColumnLayout:
//         visible: backend.lastError !== ""
//       and show it from Connections { target: backend; onErrorOccurred: ... }
//   [ ] Uncomment the backend in client-qt/main.cpp, or `backend` is undefined:
//         CameraBackend backend;
//         engine.rootContext()->setContextProperty("backend", &backend);
//   [ ] Children take it explicitly, as a required property:
//         CameraView { backend: backend }
//         Settings   { backend: backend }
//   [ ] The 500 ms refresh Timer stays in C++ for now: CameraBackend::refresh()
//       is NOT Q_INVOKABLE, so QML cannot call it. Add Q_INVOKABLE to move the
//       Timer into QML.
// ===========================================================================

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
