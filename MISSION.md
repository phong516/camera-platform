# Mission: Camera streaming from GStreamer to Qt/QML

## Why
Phong is building **camera-platform** — a multi-profile camera streaming system with a headless C++ server and a Qt/QML client. He wants to own the full path: publish camera profiles over RTSP, display a selected stream in the QML app, and understand the boundary between media playback and presentation well enough to debug and extend it himself.

## Success looks like
- Designs and implements Phase 2 himself: one capture → `tee` → per-profile `queue`/`valve`/encoder branches → RTSP media factories, verified with `gst-launch` prototypes first.
- Displays a selected RTSP profile in `CameraView.qml` and can explain which component receives the URL, decodes video, and presents frames.
- Chooses and debugs a Qt-compatible rendering path without accidentally coupling QML to server internals.
- Diagnoses pipeline symptoms (stalls, empty outputs, starved branches, CPU burn) from first principles — preroll, queues, thread boundaries — instead of trial and error.
- Reads `gst-inspect` output, element docs, and `GST_DEBUG` logs as naturally as C++ compiler errors.
- Can re-tune the same design for Orange Pi hardware (hardware encoders, constrained CPU) later.

## Constraints
- GStreamer server labs can run headlessly with `fakesink`/`filesink`; visual QML playback must be tested in an environment with a display. `/tmp` is a small tmpfs (~1.5 GB), so experiments must bound output.
- Phong already knows Qt and QML. Lessons focus on playback, RTSP, backend, and rendering boundaries rather than QML syntax basics.
- Phong writes all `camera-platform` code himself; lessons teach and give exercises, they don't produce the implementation.
- Lesson format (chosen by learner): short concept → runnable experiment with real numbers → one concrete step in the repo.
- Sessions are short; each lesson must stand alone and be completable quickly.

## Out of scope
- Web UI and cloud delivery.
- Writing custom GStreamer plugins in C.
- General Linux/CMake/tooling — already fluent.
