// ===========================================================================
// Settings.qml - bottom control strip. Spec: client-qt/UI-PROTOTYPE.md
// This is the only QML file that CALLS the backend.
//
// What CameraBackend actually offers today (CameraBackend.hpp is the truth):
//   properties : statusText, resolution, fps, streaming, bitrateKbps,
//                cpuPercent, lastError
//   invokable  : play(profile), stop(), profiles()
//
// TODO (Phase 5)
//   [ ] required property var backend
//   [ ] Source combo: there is no list property. Call the invokable once:
//         Component.onCompleted: sourceCombo.model = backend.profiles()
//       (later: a Q_PROPERTY + NOTIFY so the combo updates itself)
//   [ ] Resolution + frame rate combos are READ-ONLY in the backend.
//       UI-PROTOTYPE.md names setResolution(w,h) / setFrameRate(fps) /
//       sourceName / availableSources() / availableResolutions() - none of
//       those exist in CameraBackend.hpp yet. Add them as Q_INVOKABLE setters
//       before wiring the combos, or you are binding to nothing.
//   [ ] Start: enabled: !backend.streaming;  onClicked: backend.play()
//   [ ] Stop : enabled:  backend.streaming;  onClicked: backend.stop()
//   [ ] Disable both combos while streaming (reconfig = restart, Phase 5 call).
// ===========================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    
}
