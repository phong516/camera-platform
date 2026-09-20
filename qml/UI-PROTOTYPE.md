# QML UI Prototype (Phase 5)

This is a **drawing, not code**. No `.qml` file should be written from this until
the Qt6 backend exists. Use it as the spec for `qml/Main.qml`,
`qml/CameraView.qml`, `qml/StatusBar.qml`, `qml/Settings.qml`.

Source of truth for the C++ side: `app/CameraBackend.hpp` (`CameraBackend`), which
forwards to `Application`. QML never touches `Application`, `VideoPipeline`, or
GStreamer directly (README lines 110-119).

## Window layout

```text
┌───────────────────────────────────────────────────────────────┐
│ StatusBar.qml                                                 │
│   ● Streaming   1280x720   30 fps   12.4 Mbps   48 ms    [⚙] │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│                                                               │
│                 CameraView.qml                                │
│                 (video surface — placeholder box now)         │
│                                                               │
│                                                               │
├───────────────────────────────────────────────────────────────┤
│ Settings.qml                                                  │
│   Source      [ videotestsrc        ▾ ]                       │
│   Resolution  [ 1280x720            ▾ ]                       │
│   Frame rate  [ 30                  ▾ ]                       │
│   [ ▶ Start ]   [ ■ Stop ]                                    │
├───────────────────────────────────────────────────────────────┤
│ ErrorBanner            ⚠ camera disconnected — /dev/video0    │  (hidden unless lastError != "")
└───────────────────────────────────────────────────────────────┘
```

## Component tree

```text
ApplicationWindow (Main.qml)
├── ColumnLayout
│   ├── StatusBar        { backend }
│   ├── CameraView       { backend }        Layout.fillHeight: true
│   ├── Settings         { backend }
│   └── ErrorBanner      { text: backend.lastError }
└── Timer { interval: 500; running: true; onTriggered: backend.refresh() }
```

`Main.qml` owns the window and the refresh `Timer`. The three child components
take `backend` as a required property and bind only to it — no globals, no
`Application` access.

## Bindings contract

| QML file | Reads (backend) | Calls (backend) |
|---|---|---|
| `StatusBar.qml` | `statusText`, `resolution`, `fps`, `bitrateKbps`, `latencyMs`, `cpuPercent` | — |
| `CameraView.qml` | `streaming` | — (video surface is Phase 5 work) |
| `Settings.qml` | `sourceName`, `availableSources()`, `availableResolutions()`, `resolution`, `fps` | `setResolution(w,h)`, `setFrameRate(fps)`, `startCamera()`, `stopCamera()` |
| `Main.qml` | `lastError` | `refresh()` |

Signal wiring inside QML: `onCameraChanged` on `StatusBar`, `onErrorOccurred` on
`Main` (show `ErrorBanner`), `onStatsChanged` on `StatusBar`. The backend pushes;
QML never polls except the 500 ms `backend.refresh()` tick that re-reads C++.

Enable/disable rules:
- `Start` disabled while `backend.streaming === true`.
- `Stop` disabled while `backend.streaming === false`.
- Settings combos disabled while `backend.streaming === true` (reconfiguration
  means restarting the pipeline; that is a Phase 5 decision to revisit).

## Display states

| `statusText` | Dot colour | Notes |
|---|---|---|
| `idle` | grey | ready, not started |
| `starting` | amber | pipeline moving to PLAYING |
| `streaming` | green | frames flowing |
| `paused` | amber | paused via API |
| `error` | red | `ErrorBanner` visible |

## Prerequisite before CameraView can show frames

`CameraView.qml` starts as a placeholder rectangle with the text
"no video surface". A real surface needs one of:

1. `gstreamer1.0-qt6` + a `QQuickItem` wrapping `qmlglsink` (recommended — the
   GStreamer build here already ships the qt6 plugin path), or
2. a custom `QQuickItem` with `QSGSimpleTextureNode`, fed by `appsink` and
   uploaded via `QVideoFrame` / `QImage` on the render thread.

Option 1 needs `sudo apt install qt6-base-dev qt6-declarative-dev
gstreamer1.0-qt6`. Until then `CameraView.qml` can only be a placeholder, and
`CameraBackend.hpp` stays behind `CAMERA_PLATFORM_WITH_QT`.

## Phase 5 checklist (for QML work, later)

- [ ] `Main.qml` — window + ColumnLayout + refresh Timer + ErrorBanner
- [ ] `StatusBar.qml` — status dot + five read-only fields
- [ ] `CameraView.qml` — placeholder, then real surface once a sink exists
- [ ] `Settings.qml` — source/resolution/fps combos + Start/Stop buttons
- [ ] Install Qt6 + the qt6 GStreamer plugin, enable `CAMERA_PLATFORM_WITH_QT`
- [ ] Implement `CameraBackend.cpp` and register it as `backend` in `main()`
