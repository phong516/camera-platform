# Mission: GStreamer-powered streaming systems

## Why
Phong is building **camera-platform** — a multi-profile camera streaming system (C++ server + Qt client) that fans one capture out to HD/SD H.264 profiles over RTSP, with per-profile on/off control. The goal is to *own it end-to-end*: design it, debug it, and extend it personally — not to ship AI-generated code he can't maintain. GStreamer fluency is the bottleneck before Phase 2 (fan-out pipeline + RTSP) and the later Orange Pi deployment.

## Success looks like
- Designs and implements Phase 2 in `camera-platform` himself: one capture → `tee` → per-profile `queue`/`valve`/encoder branches → RTSP media factories, verified with `gst-launch` prototypes first.
- Diagnoses pipeline symptoms (stalls, empty outputs, starved branches, CPU burn) from first principles — preroll, queues, thread boundaries — instead of trial and error.
- Reads `gst-inspect` output, element docs, and `GST_DEBUG` logs as naturally as C++ compiler errors.
- Can re-tune the same design for Orange Pi hardware (hardware encoders, constrained CPU) later.

## Constraints
- Learning happens in WSL2 Ubuntu (headless): labs use `fakesink`/`filesink`, never display sinks. `/tmp` is a small tmpfs (~1.5 GB) — experiments must bound output with `num-buffers`/`timeout`.
- Phong writes all `camera-platform` code himself; lessons teach + give exercises, they don't produce the implementation.
- Lesson format (chosen by learner): short concept → runnable experiment with real numbers → one concrete step in the repo.
- Sessions are short; each lesson must stand alone and be completable quickly.

## Out of scope
- Qt/QML client work and web UI (separate concern).
- Writing custom GStreamer plugins in C.
- Display/rendering sinks, audio, playback apps — anything not on the streaming-server path.
- General Linux/CMake/tooling — already fluent.
