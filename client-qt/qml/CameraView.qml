// ===========================================================================
// CameraView.qml - the video surface. Currently EMPTY and NOT compiled:
// it is commented out in client-qt/qml/CMakeLists.txt.
//
// TODO (Phase 5)
//   [ ] Uncomment CameraView.qml in qml/CMakeLists.txt, and in Main.qml.
//   [ ] Stage 1 - placeholder, no dependencies:
//         import QtQuick
//         import QtQuick.Layouts
//         Rectangle {
//             required property var backend
//             color: "black"
//             Layout.fillHeight: true
//             Text {
//                 anchors.centerIn: parent
//                 color: "white"
//                 text: backend.streaming ? "streaming" : "no video surface"
//             }
//         }
//       (backend.streaming is the only backend property this file reads)
//   [ ] Stage 2 - real surface, needs a sink. Pick ONE:
//         a) gstreamer1.0-qt6 + a QQuickItem wrapping qmlglsink   (recommended)
//         b) a custom QQuickItem with QSGSimpleTextureNode, fed by appsink
//       Then set the RTSP URL from the selected profile, e.g.
//       rtsp://<host>:8554/hd
//   [ ] Nothing here links GStreamer: the client gets video over RTSP and
//       control over HTTP (docs/ARCHITECTURE-SERVER-CLIENT.md).
// ===========================================================================
