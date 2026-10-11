# Camera Platform

> A Linux-based embedded camera platform for local preview, network streaming,
> web control, and eventual cloud delivery.

The project is developed on a Linux PC first and is intended to move to an ARM
single-board computer, such as an Orange Pi, once the core pipeline is stable.

## Architecture Pivot: Server / Client Split

> **The design in this README is being superseded.** The single-process
> `camera-platform` binary (capture + GStreamer + Qt/QML + HTTP in one process) is
> being split into a headless streaming **server** and swappable **clients**.
> See [docs/ARCHITECTURE-SERVER-CLIENT.md](docs/ARCHITECTURE-SERVER-CLIENT.md) for
> the full design. This README remains the record of the pre-split plan.

The new shape:

```text
  camera-server (:8080)          supervisor, headless
        |  on/off per profile, viewer counts
        v
  camera-profile  (ONE process, ONE pipeline)
        |
        videotestsrc ! tee
          t. ! queue ! [valve ! videoscale ! x264enc ! rtph264pay]  -> :8554/hd
          t. ! queue ! [valve ! videoscale ! x264enc ! rtph264pay]  -> :8554/sd
          t. ! queue ! [valve ! videoscale ! x264enc ! rtph264pay]  -> :8554/low
        |  RTSP
        v
  client-qt   /   browser        presentation only, never links GStreamer
```

Two decisions drive the whole design:

- **One capture, fanned out by `tee`.** `tee` pushes the same buffer to every branch
  by reference, so there is no pixel copy on the fan-out and the camera is opened
  exactly once.
- **An idle profile costs ~0 CPU, switched by a `valve`.** `valve drop=true` discards
  that branch's buffers so its encoder never runs; `drop=false` resumes it. `drop` is
  changeable in any pipeline state, so profiles toggle live.

**A pipeline's state is global, so `gst_element_set_state(profilebin, PAUSED)` does
not work** - it returns success, changes nothing, and `gst_element_get_state()` on a
child reports the parent pipeline's state. The valve is the switch.

Measured on this machine (two branches, one valve closed, 5 s per phase):

| Phase | CPU | Branch A | Branch B |
| --- | --- | --- | --- |
| valve OPEN | 21.8% of one core | 30.0 fps | 30.0 fps |
| valve CLOSED | 11.0% | 30.0 fps | 0.0 fps |
| valve REOPENED | 23.6% | 30.0 fps | 30.0 fps |

CPU halves and the untoggled branch holds a steady 30 fps, so closing a branch does
not starve the tee.

This still honours the "capture and encode once, then distribute" principle: within a
profile, `shared=TRUE` on the RTSP media factory means all viewers share one encode.

## Current Focus

The first milestone is deliberately small:

```text
C++ -> GStreamer -> videotestsrc -> autovideosink -> screen
```

The initial commit should contain only:

- CMake configuration
- `main.cpp`
- A `VideoPipeline` that creates `videotestsrc -> autovideosink`

Everything else in this document is a direction for later increments, not a
requirement to implement the full platform immediately.

## Goals

- Learn GStreamer and Linux multimedia
- Build a clean C++/Qt/QML architecture
- Understand V4L2 camera integration
- Build a reusable video pipeline
- Stream video to multiple destinations
- Develop the software on a PC before moving to embedded hardware

---

## Architecture

The high-level architecture is:

```text
                         ┌──────────────────┐
                         │      Cloud       │
                         │   Video Stream   │
                         └────────▲─────────┘
                                  │
                                  │ Network
                                  │
┌──────────────┐          ┌───────┴────────┐
│ Video Source │          │    Streaming   │
│              │          │                │
│ videotestsrc │          │ RTSP / WebRTC  │
│     ↓        │          │ Cloud streamer │
│   v4l2src    │          └───────▲────────┘
│   (later)    │                  │
└──────┬───────┘                  │
       │                          │
       │ raw / encoded video      │
       ▼                          │
┌─────────────────────────────────┴─────────┐
│              Video Pipeline               │
│                                           │
│ Capture → Convert → Encode → Tee → Output │
└────────────────────┬──────────────────────┘
                     │
                     ├──────────► Qt/QML
                     │             │
                     │             ▼
                     │        Local Display
                     │
                     └──────────► Network
                                   │
                              ┌────┴─────┐
                              │          │
                              ▼          ▼
                             Web       Cloud
```

### Software Layers

```text
┌────────────────────────────────────┐
│              Qt / QML              │
│          Presentation Layer        │
└─────────────────▲──────────────────┘
                  │
┌─────────────────┴──────────────────┐
│               C++                  │
│        Application / Control       │
│                                    │
│ CameraState                        │
│ SystemStatus                       │
│ WebServer                          │
│ StreamServer                       │
└─────────────────▲──────────────────┘
                  │
┌─────────────────┴──────────────────┐
│             GStreamer              │
│             Media Engine           │
│                                    │
│ Capture → Convert → Encode → Tee   │
└─────────────────▲──────────────────┘
                  │
          ┌───────┴────────┐
          │                │
    videotestsrc        v4l2src
       (PC)           (USB camera)
```

### Design Principles

- GStreamer owns the media pipeline.
- C++ owns application logic and control flow.
- QML owns presentation.
- QML does not manipulate GStreamer objects directly.
- The web API does not depend directly on GStreamer internals.
- Video should be captured and encoded once, then distributed to outputs.
- Hardware-specific functionality stays isolated so the application can move to
     an ARM SBC.

## Project Structure

> The tree below reflects the **server / client split**, which is implemented.
> See [docs/ARCHITECTURE-SERVER-CLIENT.md](docs/ARCHITECTURE-SERVER-CLIENT.md).

```text
camera-platform/
│
├── CMakeLists.txt              # finds deps, adds the subdirectories
│
├── common/                     # shared, header-only (no Gst, no Qt)
│   ├── Protocol.hpp            # ports, API paths, mount names, state names
│   └── README.md
│
├── model/                      # CameraState, SystemStatus (value types)
│
├── web/                        # HTTP server + message parsing (POSIX sockets)
│   ├── WebServer.{hpp,cpp}     # cpp empty until Phase 3
│   ├── ApiController.{hpp,cpp} # cpp empty until Phase 3
│   └── HttpMessage.hpp
│
├── video/                      # GStreamer pipeline + video sources
│   ├── VideoPipeline.{hpp,cpp}
│   ├── VideoSource.hpp
│   ├── TestVideoSource.{hpp,cpp}
│   └── CameraVideoSource.{hpp,cpp}
│
├── server/                     # -> camera-server   (headless, never links Qt)
│   ├── main.cpp                # gst_init / run / gst_deinit
│   ├── Application.{hpp,cpp}   # pipeline + GMainLoop + signal sources
│   └── CloudStreamer.hpp       # interface only, Phase 8
│
├── client-qt/                  # -> camera-client-qt (never links GStreamer)
│   ├── main.cpp                # QQmlApplicationEngine, exposes `backend`
│   ├── UI-PROTOTYPE.md
│   └── qml/                    # QML module "CameraPlatform" (STATIC)
│       ├── CameraBackend.{hpp,cpp}   # QML bridge: JSON -> properties
│       ├── Main.qml
│       ├── StatusBar.qml
│       ├── CameraView.qml      # still empty, not yet in the module
│       └── Settings.qml        # still empty, not yet in the module
│
├── tools/                      # camera-cli lands in Phase 3
│
├── cmake/
│   ├── FindGStreamer.cmake
│   └── FindGStreamerRtspServer.cmake
│
├── docs/
│   └── ARCHITECTURE-SERVER-CLIENT.md
│
└── tests/                      # no framework; see TestSupport.hpp
    ├── video/test_videopipeline.cpp
    └── web/test_http.cpp
```

The `app/`, `qml/`, `streaming/`, and `system/` directories from the pre-split tree
are gone: `app/` became `server/` + `client-qt/`, and `streaming/` + `system/` were
empty stubs.

Includes are written **repo-root-relative** (`"server/Application.hpp"`,
`"video/VideoPipeline.hpp"`), so each target puts `${PROJECT_SOURCE_DIR}` on its
include path explicitly.

### Building

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

./build/server/camera-server          # headless; needs a display for autovideosink
./build/client-qt/camera-client-qt    # the Qt client
```

The server has no Qt dependency at all, so a headless or embedded build is:

```bash
cmake -S . -B build -DCAMERA_PLATFORM_WITH_QT=OFF
cmake --build build -j
```

## Development Roadmap

### Phase 1 — GStreamer Fundamentals

Start with a synthetic video source.

videotestsrc
      │
      ▼
autovideosink
      │
      ▼
    Screen

Example:
gst-launch-1.0 videotestsrc ! autovideosink

Goals:

- Understand GStreamer elements and pipelines
- Understand pads, caps, and linking
- Understand pipeline states and the GStreamer bus
- Understand messages and errors

### Phase 2 — GStreamer from C++

Replace gst-launch-1.0 with a C++ application.

C++
 │
 ▼
VideoPipeline
 │
 ▼
GStreamer
 │
 ▼
videotestsrc
 │
 ▼
autovideosink

Goals:

- Initialize GStreamer
- Create and link elements
- Change pipeline state
- Handle bus messages and errors
- Shut down the pipeline cleanly

### Phase 3 — Video Format and Encoding

Build a controlled video pipeline.

videotestsrc
     │
     ▼
video/x-raw
 1280x720@30
     │
     ▼
Converter
     │
     ▼
H.264 Encoder
     │
     ▼
H.264 stream

Goals:

- Understand raw video formats and caps negotiation
- Understand frame rate and resolution
- Understand H.264 encoding
- Compare software and hardware encoding

### Phase 4 — Network Streaming

Introduce network streaming.

videotestsrc
     │
     ▼
H.264 Encoder
     │
     ▼
RTSP / WebRTC
     │
     ▼
Another machine

Goals:

- Understand encoded video streams, RTSP, and WebRTC
- Measure latency and bitrate
- Understand buffering and network failures

### Phase 5 — Qt/QML Integration

Integrate the video pipeline with the Qt/QML application.

GStreamer

GStreamer
    │
    ▼
C++ Backend
    │
    ▼
Qt/QML
    │
    ▼
Local Display

The QML layer should eventually expose:

- Start and stop
- Resolution and frame rate
- Stream status
- FPS and bitrate
- Error status

### Phase 6 — Web Server

Add an HTTP control interface.

Example API:

GET  /api/status
POST /api/camera/start
POST /api/camera/stop
GET  /api/config
POST /api/config

Architecture:

Browser
   │
   ▼
WebServer
   │
   ▼
Application
   │
   ▼
VideoPipeline

The web server controls the application through a clean interface rather than directly manipulating GStreamer.

### Phase 7 — Multiple Outputs

The pipeline should support multiple destinations.

                       ┌──► Local Display
                       │
Camera → Encode → Tee ─┼──► Network Stream
                       │
                       └──► Cloud Stream

The goal is to avoid unnecessary duplicate encoding.

For example, prefer:

Camera
  ↓
Encode once
  ↓
Tee
 ├──► Output 1
 ├──► Output 2
 └──► Output 3

 instead of:

 Camera
 ├──► Encoder 1
 ├──► Encoder 2
 └──► Encoder 3

### Phase 8 — Cloud Streaming

Add a cloud streaming backend.

The exact protocol/service will be selected after the local streaming pipeline is working.

Possible technologies include:

WebRTC
RTSP
RTMP/RTMPS
SRT
HLS

The primary objective is low-latency and reliable remote video delivery.

### Phase 9 — Real USB Camera

Replace the synthetic source:

videotestsrc

with:

v4l2src

Architecture:

USB Camera
    │
    ▼
   V4L2
    │
    ▼
GStreamer
    │
    ▼
Video Pipeline

Goals:

- Understand `/dev/video*` and V4L2
- Enumerate camera capabilities
- Select resolution, pixel format, and frame rate
- Handle camera disconnect and reconnect

Example investigation:

v4l2-ctl --list-devices
v4l2-ctl --list-formats-ext
### Phase 10 — ARM SBC Deployment

After the PC implementation is stable, deploy the application to an Orange Pi.

Development PC
      │
      │ Build / Deploy
      ▼
 Orange Pi
      │
      ├── USB Camera
      ├── Wi-Fi
      ├── Ethernet
      ├── HDMI
      └── Touchscreen

At this stage investigate:

ARM64
V4L2 drivers
DRM/KMS
Hardware video acceleration
Hardware H.264 encoding
GStreamer platform plugins
GPU/VPU support
Network management
systemd
CPU/memory usage
Thermal behavior
Power consumption

## Build Environment

The initial development target is Linux PC.

Expected dependencies:

- CMake
- GCC or Clang
- Qt 6
- GStreamer 1.x
- GStreamer development packages

On Debian/Ubuntu/Pop!_OS:

```bash
sudo apt update
sudo apt install \
    build-essential \
    cmake \
    pkg-config \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
     gstreamer1.0-libav
```

Verify:

```bash
gst-launch-1.0 --version
gst-inspect-1.0 videotestsrc
```

### Building with the QML UI

Qt6 is optional. The build looks for it automatically:

```bash
sudo apt install qt6-base-dev qt6-declarative-dev

cmake -S . -B build                                  # AUTO: QML UI if Qt6 is found
cmake -S . -B build -DCAMERA_PLATFORM_WITH_QT=ON     # require Qt6, fail if missing
cmake -S . -B build -DCAMERA_PLATFORM_WITH_QT=OFF    # console app only
cmake --build build -j
```

With Qt6 enabled, `client-qt/qml/*.qml` is compiled into a `CameraPlatform` QML
module and embedded in the **`camera-client-qt`** binary as Qt resources. The
server (`camera-server`) is built either way and never links Qt.

### First Test

Before building the C++ application, verify GStreamer independently:

```bash
gst-launch-1.0 videotestsrc ! autovideosink
```

Specify a resolution and frame rate:

```bash
gst-launch-1.0 \
    videotestsrc \
    ! video/x-raw,width=1280,height=720,framerate=30/1 \
     ! autovideosink
```

If the test pattern appears, the basic GStreamer installation is working.

## Current Development Strategy

The project intentionally starts without a physical camera.

The initial source is:

videotestsrc

The eventual source will be:

v4l2src

This allows the software architecture and most of the media pipeline to be developed before purchasing hardware.

The intended progression is:

videotestsrc
     ↓
GStreamer
     ↓
C++
     ↓
H.264
     ↓
RTSP/WebRTC
     ↓
Qt/QML
     ↓
Web API
     ↓
Cloud
     ↓
v4l2src
     ↓
Orange Pi

## Non-Goals

The project will not initially attempt to solve everything at once.

The following are intentionally postponed:

- Hardware-specific video acceleration
- Wi-Fi AP mode
- D-Bus NetworkManager integration
- Cloud provider selection
- Hardware-specific GPIO
- Production security
- OTA updates
- Containerization
- AI and computer vision

These will be considered only after the basic video pipeline is stable.

## Learning Goals

This project is also a practical embedded Linux learning project.

It is intended to provide experience with:

- Linux processes, threads, file descriptors, and device interfaces
- V4L2, networking, and systemd
- GStreamer elements, pads, caps, pipelines, buses, buffers, encoders,
     decoders, and tees
- C++ RAII, interfaces, ownership, concurrency, event-driven programming, and
     error handling
- CMake and Qt/QML, including `QObject`, signals and slots, properties,
     model/view, and C++ integration
- HTTP, REST, WebSocket, RTSP, WebRTC, and streaming protocols
- Embedded Linux and network failure handling

Eventually:

- ARM64, device trees, kernel drivers, and V4L2
- DRM/KMS and hardware acceleration
- Cross-compilation and systemd services
- Performance optimization

## Design Philosophy

The project follows a few rules:

Keep hardware-specific code isolated

Application
    │
    ▼
VideoPipeline
    │
    ▼
VideoSource
    │
    ├── TestVideoSource
    └── V4L2VideoSource

Keep UI independent from media implementation

QML
 ↓
C++ Application
 ↓
VideoPipeline
 ↓
GStreamer

Prefer measurement over assumptions

Performance should be measured using:

CPU usage
Memory usage
FPS
Bitrate
End-to-end latency
Dropped frames
Network throughput

Especially when moving to an ARM SBC, the existence of a hardware video engine does not guarantee that Linux/GStreamer can actually use it efficiently.

## Status

Project status:

- [✅] Project skeleton
- [✅] GStreamer installation
- [✅] `videotestsrc` preview
- [✅] C++ GStreamer pipeline
- [✅] Video format control
- [✅] Server / client split (repo restructured, two targets build)
- [ ] H.264 encoding
- [ ] Network streaming (RTSP, one mount per profile)
- [ ] Tee + valve profile switching (mechanism measured, not yet built)
- [ ] Qt/QML client (window loads; no backend wired yet)
- [ ] Web control page
- [ ] Browser video (WebRTC bridge)
- [ ] Cloud streaming
- [ ] USB camera
- [ ] ARM64 build
- [ ] Orange Pi deployment

The immediate target is now:

```text
camera-profile  ->  tee + valve  ->  GstRtspServer :8554/hd  ->  Qt client
```

i.e. get two profiles sharing one capture, streaming over RTSP, and played back by
the client - then confirm that closing one profile's valve drops CPU while the other
keeps playing. The old target below (`videotestsrc` on screen in one
process) is already done and is kept only as the historical starting point.

```text
C++ -> GStreamer -> videotestsrc -> screen
```
