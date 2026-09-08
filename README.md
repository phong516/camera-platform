# Camera Platform

> A Linux-based embedded camera platform for local preview, network streaming,
> web control, and eventual cloud delivery.

The project is developed on a Linux PC first and is intended to move to an ARM
single-board computer, such as an Orange Pi, once the core pipeline is stable.

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

```text
camera-platform/
│
├── CMakeLists.txt
├── cmake/
│   └── FindGStreamer.cmake
│
├── app/
│   ├── main.cpp
│   ├── Application.hpp
│   └── Application.cpp
│
├── video/
│   ├── VideoPipeline.hpp
│   ├── VideoPipeline.cpp
│   │
│   ├── VideoSource.hpp
│   ├── TestVideoSource.hpp
│   ├── TestVideoSource.cpp
│   ├── CameraVideoSource.hpp
│   └── CameraVideoSource.cpp
│
├── streaming/
│   ├── StreamServer.hpp
│   ├── StreamServer.cpp
│   ├── CloudStreamer.hpp
│   └── CloudStreamer.cpp
│
├── web/
│   ├── WebServer.hpp
│   ├── WebServer.cpp
│   ├── ApiController.hpp
│   └── ApiController.cpp
│
├── system/
│   ├── NetworkManager.hpp
│   └── NetworkManager.cpp
│
├── model/
│   ├── CameraState.hpp
│   └── SystemStatus.hpp
│
├── qml/
│   ├── Main.qml
│   ├── CameraView.qml
│   ├── StatusBar.qml
│   └── Settings.qml
│
└── tests/
    ├── video/
    └── web/
```

Not all components are implemented yet. The project is developed incrementally.

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

- [ ] Project skeleton
- [ ] GStreamer installation
- [ ] `videotestsrc` preview
- [ ] C++ GStreamer pipeline
- [ ] Video format control
- [ ] H.264 encoding
- [ ] Network streaming
- [ ] Qt/QML integration
- [ ] Web server
- [ ] Multiple outputs
- [ ] Cloud streaming
- [ ] USB camera
- [ ] ARM64 build
- [ ] Orange Pi deployment

The immediate target is:

```text
C++ -> GStreamer -> videotestsrc -> screen
```

Everything else comes after that.
