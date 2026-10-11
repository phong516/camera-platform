# Camera Platform - Server / Client Split

> Status: **Phase 1 (restructure) is implemented and the build is green.**
> The profile pipeline itself (Phase 2) is not built yet.
>
> Revision 3 adds the implemented tree in section 5. Revision 2 changed the profile
> pool to **one shared pipeline with a valve per profile**, not one process per
> profile; section 3 carries the measurements that justify it.
>
> Supersedes the single-process `camera-platform` design in `README.md` (Phases 4-7).

## 1. Why the split

Today one binary does everything: capture, GStreamer pipeline, Qt/QML display, HTTP
API. That couples three things with very different lifetimes:

| Concern | Lifetime | Needs |
| --- | --- | --- |
| Capture + encode + stream | long-running, headless | GStreamer, no GUI |
| Control plane (HTTP API) | long-running, headless | HTTP, no GStreamer objects |
| Presentation | per user, anywhere | Qt or a browser, no GStreamer knowledge |

The split makes the server **headless** (so it can run as a systemd service on the
Orange Pi with no display attached) and makes the client **swappable** (Qt today,
browser alongside it). The client never links GStreamer.

## 2. Target architecture

```text
                    +-----------------------------------------------+
                    |  camera-server  (supervisor, headless)        |
                    |                                               |
                    |   web/WebServer  -->  ProfileManager          |
                    |   :8080  REST API      |  profile on/off,     |
                    |   + static HTML page   |  state, viewer count |
                    +------------------------+----------------------+
                                             | control socket
                                             v
                    +-----------------------------------------------+
                    |  camera-profile  (ONE process, ONE pipeline)  |
                    |                                               |
                    |   videotestsrc ! tee                          |
                    |     t. ! queue ! BIN[valve!scale!x264!pay] /hd  :8554
                    |     t. ! queue ! BIN[valve!scale!x264!pay] /sd  :8554
                    |     t. ! queue ! BIN[valve!scale!x264!pay] /low :8554
                    |     t. ! queue ! BIN[valve!scale!x264!pay] /thm :8554
                    |                                               |
                    |   ONE GstRtspServer on :8554, one mount each   |
                    +-----------------------------------------------+
                                             ^
                                             | RTSP (video)
                    +------------------------+----------------------+
                    |                     clients                   |
                    |                                               |
                    |   camera-client-qt      browser               |
                    |   (GStreamer/Qt MM)     (control page;        |
                    |                         WebRTC in Phase 7)    |
                    +-----------------------------------------------+
                              |                        |
                              +---- HTTP control ------+
                                     :8080
```

Two planes, deliberately separate:

- **Control plane - HTTP/JSON on `:8080`.** Clients talk to the server for decisions.
  Small, cacheable, easy to debug with `curl`.
- **Media plane - RTSP on `:8554`.** One port, one mount point per profile. The
  server is not in the video path at all; an already-playing stream survives a
  control-plane restart.

**Changed from revision 1:** one process and one pipeline hold all profiles. The
profiles are branches of a `tee`, each switched by a `valve`. This means one camera
open, one capture, and zero-copy fan-out to every profile.

## 3. The core mechanism: switching a profile off

### 3.1 GStreamer pipeline state is global - this is the constraint that shapes the design

A pipeline has exactly **one** state. You cannot set one branch to `PAUSED` while
another branch plays:

```
gst_element_set_state(profilebin1, GST_STATE_PAUSED)
```

This **does not work, and does not tell you it failed.** Measured on this machine:

```
B) gst_element_set_state(queue, GST_STATE_PAUSED)
   return=1          <- GST_STATE_CHANGE_SUCCESS, it claims success
   queue =PLAYING    <- unchanged
```

The upstream documentation is explicit: *"If the element is not a top-level element,
the state change is refused."* Worse, `gst_element_get_state()` on a child returns
the **parent pipeline's** state, so a UI that reads branch state would display
`PLAYING` for a branch that is supposed to be off. Do not build the design on
per-bin state.

**The switch is a valve, not a state.**

### 3.2 The valve pattern

```text
videotestsrc ! tee name=t
   t. ! queue ! valve name=v1 ! videoscale ! x264enc ! rtph264pay ! ...   (hd)
   t. ! queue ! valve name=v2 ! videoscale ! x264enc ! rtph264pay ! ...   (sd)
   t. ! queue ! valve name=v3 ! videoscale ! x264enc ! rtph264pay ! ...   (low)
```

- `valve drop=false` - branch runs, encoder works
- `valve drop=true`  - branch discards buffers, encoder never runs, ~0 CPU
- `drop` is **changeable in any state**, so you toggle it live on a PLAYING pipeline
- No state change on any element, so nothing can stall the pipeline

Each profile is a self-contained bin with a ghost sink pad, so the pipeline reads as
`tee ! profilebin_hd`, `tee ! profilebin_sd`, and each bin owns its valve, scaler,
encoder, and payloader.

### 3.3 Measured evidence

A two-branch test on this machine: `tee -> [queue ! x264enc A] + [queue ! valve !
x264enc B]`, 320x240 (the `videotestsrc` default), 5 seconds per phase.

| Phase | CPU | Branch A | Branch B |
| --- | --- | --- | --- |
| valve OPEN (both encoding) | 21.8% of one core | 30.0 fps | 30.0 fps |
| **valve CLOSED (B idle)** | **11.0%** | 30.0 fps | **0.0 fps** |
| valve REOPENED | 23.6% | 30.0 fps | 30.0 fps |

Three things this proves:

1. **CPU halves** when one of two encoders is switched off - the saving is real.
2. **Branch A held a steady 30.0 fps throughout.** The tee did not stall, and the
   queue never backed up (`0/200` buffers). This was the main risk of the shared
   pipeline and it does not materialise.
3. **B recovers immediately on reopen.**

The absolute CPU numbers are small because the test ran at 320x240. The ratio is what
matters, and it scales with the number of branches switched off.

### 3.4 Element order: the one rule that matters

The valve must be **in the chain feeding that branch's encoder**. Both of these work
identically - measured, 11.0% vs 11.2%:

```text
t. ! queue ! valve ! x264enc     <- works
t. ! valve ! queue ! x264enc     <- works
```

A valve accepts incoming buffers and drops them, returning OK upstream. That is why
the tee never blocks. **What would block the tee is a pad block or a pad probe on the
branch path** - the streaming thread stalls there, and the tee pushes synchronously
to every pad, so one stalled pad stalls all profiles. Use a valve; do not use a pad
probe to disable a branch.

### 3.5 Profile state machine

The server drives this; a client only reads it.

| State | Branch | CPU | Entered when |
| --- | --- | --- | --- |
| `off` | valve `drop=true` | ~0 | profile disabled, or 0 viewers |
| `ready` | valve `drop=false`, no viewer | 1 encode | profile enabled, nobody watching |
| `live` | valve `drop=false`, viewers attached | 1 encode | >=1 RTSP client on the mount |

Note the deliberate difference from revision 1: because all profiles share one
pipeline, a profile cannot enter a true `PAUSED`. `valve drop=true` gives the same
CPU result (encoder not running) while the pipeline stays `PLAYING`.

A profile with `drop=false` and no viewers still encodes and discards. That is wasted
work, so **the server closes the valve whenever a profile's viewer count reaches
zero** - which is exactly the pause-when-idle goal. The default is off-when-idle.

### 3.6 Two ways a profile becomes `live`

- **Pre-warmed.** Valve opens when the process starts; the branch encodes from
  startup. Tune-in is instant, but it burns CPU with nobody watching. Only for a
  profile you always want hot.
- **Lazy (recommended default).** Valve stays closed until the first viewer requests
  the mount, then opens. Costs ~100-300 ms of settle time before the first frames
  (measured in Phase 8), saves CPU the entire rest of the time.

Lazy is the default. Pre-warm at most one profile, and only if a fast first frame
matters more than idle CPU.

## 4. Profile catalog

Profiles are data, not code. Kept in the server, exposed read-only to clients.

| Name | Resolution | FPS | Bitrate | Mount |
| --- | --- | --- | --- | --- |
| `hd` | 1920x1080 | 30 | 4000 kbps | `rtsp://127.0.0.1:8554/hd` |
| `sd` | 1280x720 | 30 | 2000 kbps | `rtsp://127.0.0.1:8554/sd` |
| `low` | 854x480 | 30 | 800 kbps | `rtsp://127.0.0.1:8554/low` |
| `thumb` | 640x360 | 15 | 300 kbps | `rtsp://127.0.0.1:8554/thumb` |

- **One port (`:8554`), one mount point per profile.** One `GstRtspServer` serves
  them all. This is cleaner than one port per profile and matches the single-process
  design.
- Each branch downscales from the shared source with `videoscale`, so the source runs
  at the highest resolution and every profile is independent.
- `videotestsrc` now; `v4l2src` (Phase 9) later, swapped by changing the source
  fragment only. One camera open is now guaranteed.
- Encoder: `x264enc speed-preset=ultrafast tune=zerolatency key-int-max=30`.
- The RTSP media factory must set `shared=TRUE` so that *multiple viewers of the same
  profile* share one encoder rather than each spawning a pipeline. The default is
  `FALSE`, which silently defeats this.

### 4.1 What this design fixed

Revision 1 accepted "N profiles = N captures" as a cost of process isolation: N opens
of `/dev/video*`, N raw conversions, and on some single-camera embedded boards the
driver refuses the second open outright.

The shared pipeline removes that entirely. `tee` pushes the *same* buffer to every
branch, refcounted - no pixel copy on the fan-out. One camera open, one capture. Only
the per-branch `videoscale` + `x264enc` cost is multiplied, and `x264enc` dominates
both.

### 4.2 What it costs

- **Shared fate.** One pipeline means a crash takes every profile down. The
  supervisor restarts the process; all streams drop briefly. Accepted for now.
- **No true per-profile `PAUSED`.** Covered by `valve drop=true` (section 3.5).
- **`videoscale` per non-native resolution.** Cheap relative to encoding, but real.

If per-profile crash isolation or true `PAUSED` later becomes necessary, the fallback
is separate encoder processes fed by one capture process over shared memory
(`interpipe`, or `shmsink`/`shmsrc`). Note `gst-interpipe` is **not installed** on
this machine; it would have to be built. Do not start there.

## 5. Repository layout

One repo, two top-level targets, plus a thin shared layer. `common/` holds only
header-only value types and small utilities - no GStreamer, no Qt - so the dependency
graph stays one-directional:

```text
common/  <-- server/ , client-qt/ , tools/
```

```text
camera-platform/
|-- CMakeLists.txt              # finds deps (incl. gstreamer-rtsp-server), adds subdirs
|
|-- common/                     # shared, header-only, no Gst, no Qt
|   |-- CMakeLists.txt          # INTERFACE target `common`
|   |-- Protocol.hpp            # ports, API paths, mount names, state names
|   `-- README.md               # rules for what may live here
|
|-- model/                      # CameraState, SystemStatus (value types)
|   `-- CMakeLists.txt          # INTERFACE target `model_lib`
|
|-- web/                        # HTTP server + message parsing (POSIX sockets)
|   |-- CMakeLists.txt          # static lib `web_lib`
|   |-- WebServer.{hpp,cpp}     # cpp empty until Phase 3
|   |-- ApiController.{hpp,cpp} # cpp empty until Phase 3
|   `-- HttpMessage.hpp
|
|-- video/                      # GStreamer pipeline + sources
|   |-- CMakeLists.txt          # static lib `video_lib`
|   |-- VideoPipeline.{hpp,cpp} # single-sink pipeline; Phase 2 adds ProfileHost
|   |-- VideoSource.hpp
|   |-- TestVideoSource.{hpp,cpp}
|   `-- CameraVideoSource.{hpp,cpp}
|
|-- server/                     # -> camera-server   (headless, never links Qt)
|   |-- CMakeLists.txt          # static lib `camera_server_lib` + executable
|   |-- main.cpp                # gst_init / run / gst_deinit, no QApplication
|   |-- Application.{hpp,cpp}   # owns the pipeline + GMainLoop + signal sources
|   `-- CloudStreamer.hpp       # interface only, Phase 8
|
|-- client-qt/                  # -> camera-client-qt (never links GStreamer)
|   |-- CMakeLists.txt          # qt_standard_project_setup + executable
|   |-- main.cpp                # QQmlApplicationEngine, exposes `backend`
|   |-- UI-PROTOTYPE.md         # wireframe + property/slot contract
|   `-- qml/                    # QML module URI CameraPlatform (STATIC)
|       |-- CMakeLists.txt      # qt_add_qml_module
|       |-- CameraBackend.{hpp,cpp}  # QML bridge: JSON snapshot -> properties
|       |-- Main.qml
|       |-- StatusBar.qml
|       |-- CameraView.qml      # still empty, not yet in the module
|       `-- Settings.qml        # still empty, not yet in the module
|
|-- tools/                      # camera-cli lands in Phase 3
|   `-- CMakeLists.txt
|
|-- cmake/
|   |-- FindGStreamer.cmake
|   `-- FindGStreamerRtspServer.cmake
|
|-- docs/
|   `-- ARCHITECTURE-SERVER-CLIENT.md   # this file
|
`-- tests/                      # no framework; see TestSupport.hpp
    |-- CMakeLists.txt
    |-- TestSupport.hpp
    |-- video/test_videopipeline.cpp
    `-- web/test_http.cpp
```

Verified after the restructure (`cmake -S . -B build && cmake --build build -j8`):

| Check | Result |
| --- | --- |
| `camera-server` builds | yes |
| `camera-client-qt` builds | yes |
| `ctest` | 2/2 pass |
| `camera-server` links Qt | **no** (0 libQt6 entries) |
| `camera-client-qt` links Qt | yes (6 libQt6 entries) |
| `-DCAMERA_PLATFORM_WITH_QT=OFF` | configures and builds `camera-server` alone |
| `camera-server` runs with no display | yes, stays up until signalled |
| `camera-client-qt` loads its QML | yes, verified with `QT_QPA_PLATFORM=offscreen` (no QML errors) |
| gstreamer-rtsp-server detected | 1.28.2 |
| configure warnings | 3, all Qt `qtquickcontrols2*styleplugin` ("will not be linked"); cosmetic, see below |

One Qt quirk worth keeping: the QML plugin targets live in separate packages under
`Qt6Qml/QmlPlugins` and are **not** loaded by `find_package(Qt6 COMPONENTS ...)`.
The top level therefore asks for the umbrella `QmlPlugins` component. Without it,
`qt_import_qml_plugins()` emits one warning per plugin (19 of them). Three style
plugins still warn; the client was verified to load its QML module and run with no
errors under `QT_QPA_PLATFORM=offscreen`, so they are warnings, not gaps.

Build step for anyone picking this up:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/server/camera-server        # needs a display for autovideosink
```

Moves from the current tree:

| Before | After | Status |
| --- | --- | --- |
| `app/` (Qt + GStreamer mixed) | split into `server/` + `client-qt/`; two mains | **done**, `app/` deleted |
| `app/CameraBackend.*` | `client-qt/qml/CameraBackend.*` | **done**, now a real class with a JSON entry point |
| `qml/*.qml` | `client-qt/qml/*.qml` | **done** |
| `streaming/StreamServer.*` | superseded by `server/RtspProfileServer.*` (Phase 2) | **deleted** (was an empty stub) |
| `streaming/CloudStreamer.{hpp,cpp}` | `server/CloudStreamer.hpp`, empty `.cpp` dropped | **done** |
| `system/NetworkManager.*` | *(deferred)* | **deleted** (empty stub; Phase 9) |
| `streaming/`, `system/` directories | gone | **done** |
| `video/VideoPipeline.*` | reused now; `server/ProfileHost.*` supersedes it in Phase 2 | pending Phase 2 |
| `video/VideoSource.*`, `web/`, `model/` | unchanged, now real CMake targets | **done** |

Things that changed shape during the restructure, worth knowing:

- **`client-qt` does not link the server.** Revision 1 assumed `CameraBackend`
  would wrap `Application`. It now communicates over HTTP instead, so the client
  target was deliberately rebuilt without `camera_server_lib`. Phase 4 wires
  `ServerClient` into `refresh()`.
- **`CameraBackend` is a working class, not a stub.** It parses the
  `/api/profiles` JSON shape into the QML properties and emits change signals;
  `play()`/`stop()`/`refresh()` are the Phase 4 seams.
- **`common/` is real now.** `Protocol.hpp` holds the ports, API paths, mount
  names, and profile state names so the server, the Qt client, and the browser
  page cannot drift apart. It has no GStreamer and no Qt, which is what keeps the
  server headless.
- **Repo-root-relative includes are the convention** (`"server/Application.hpp"`,
  `"video/VideoPipeline.hpp"`). Targets therefore put `${PROJECT_SOURCE_DIR}` on
  their include path explicitly; there is no top-level `include_directories()`
  making it implicit.

## 6. Control plane API

`camera-server`, `:8080`. JSON in, JSON out. `/` serves the static control page.

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/api/profiles` | catalog + live state of every profile |
| `GET` | `/api/profiles/{name}` | one profile |
| `POST` | `/api/profiles/{name}/on` | open the valve; idempotent |
| `POST` | `/api/profiles/{name}/off` | close the valve; frees the encoder |
| `GET` | `/api/status` | host + supervisor health (`SystemStatus`) |
| `GET` | `/api/health` | liveness for systemd/scripts |

`GET /api/profiles` - the one response the clients depend on:

```json
{
  "profiles": [
    {
      "name": "hd",
      "width": 1920, "height": 1080, "fps": 30, "bitrateKbps": 4000,
      "state": "live",
      "url": "rtsp://127.0.0.1:8554/hd",
      "viewers": 1,
      "cpuPercent": 18.4,
      "lastError": ""
    }
  ]
}
```

Design rules:

- **The server owns state; clients never guess.** A client reads `state` before
  showing a play button, and re-reads it after a disconnect.
- **`on`/`off` are idempotent** - `200` whether it changed anything or not.
- **Errors are data, not HTTP failures**: a pipeline error reports `state: "error"`
  plus `lastError`, so one bad profile never breaks the list.
- **No auth yet** (localhost only). A Phase 9 hardening item, not a gap.

## 7. Client design

### 7.1 Qt client

`QMediaPlayer` + `QVideoSink` fed the RTSP URL via the `gstreamer` Qt Multimedia
backend that is already installed. `rtspsrc` is far more mature than Qt's own RTSP
handling, and Qt on Linux uses GStreamer underneath anyway. Falling back to a
hand-rolled `QQuickFramebufferObject` + `qml6glsink` pipeline is the escape hatch if
Qt Multimedia's latency or control proves insufficient.

Priorities:

1. Load profiles from the API and render a selector.
2. Play the selected profile; show `state`, bitrate, and resolution.
3. Treat EOS/disconnect as normal - the server may close the valve when the viewer
   count hits zero. Reconnect or offer another profile, do not surface an error.
4. Never link GStreamer directly.

A headless `--dump-profile hd` mode (print the RTSP URL and exit) keeps the client
testable in CI and scriptable without a display.

### 7.2 Browser client

Phase 5 ships the control page only (list profiles, on/off, live state, no video).
RTSP is not playable in a browser, so video arrives in Phase 7 as a WebRTC bridge
(`webrtcbin` is already installed). This is why RTSP is the internal protocol: it is
the GStreamer-native path for the Qt client, and the browser gets a separate bridge
rather than forcing the whole design toward WebRTC.

## 8. Migration plan

Each phase leaves the build green and the previous behaviour working, so the approach
can be abandoned at any phase without a broken tree.

**Phase 0 - Documentation only.** This file + README update. No code changes.

**Phase 1 - Skeleton and shared layer. DONE.** `common/`, `server/`, `client-qt/`,
and `tools/` exist; the old `app/`, `qml/`, `streaming/`, and `system/` trees are
gone; `camera-server` and `camera-client-qt` are separate targets; the server links
no Qt and the client links no GStreamer; `ctest` is 2/2. See the verification table
in section 5. The old binary is intentionally *not* kept: the split is now the only
build.

**Phase 2 - One pipeline, two branches, one valve.** Build `ProfileHost` +
`ProfileBin` with two profiles. Serve over RTSP with one `GstRtspServer`
(`shared=TRUE`). Verify with `gst-launch-1.0 playbin uri=rtsp://127.0.0.1:8554/hd`,
then close the valve and confirm CPU drops while the other profile keeps playing
(expect roughly half, per section 3.3). **This is the phase that de-risks everything.**

**Phase 3 - Catalog, manager, API.** All profiles from the catalog, viewer counting,
auto-close valve at zero viewers, `ControlApi` exposing section 6. Delete the old
`app/`.

**Phase 4 - Qt client.** `ServerClient` + `StreamPlayer` + profile picker. First
end-to-end demo: pick a profile, watch it, see the others sit at ~0 CPU.

**Phase 5 - Browser control page.** Static page served by the server: list, on/off,
live state. No video.

**Phase 6 - Auto-off tuning.** Grace period before closing a valve on viewer
disconnect, so brief reconnects do not thrash the encoder.

**Phase 7 - WebRTC bridge** for browser video.

**Phase 8 - Measurement.** Per-profile CPU at each resolution, tune-in latency,
`videoscale` overhead per branch, actual savings with N branches off. Feeds Phase 9
sizing.

**Phase 9 - Embedded.** `videotestsrc` -> `v4l2src`, hardware encoder (which changes
the encoder element per branch, not the topology), systemd unit, and the
per-profile-isolation question from section 4.2 revisited against real hardware.

## 9. Open questions

1. **Auto-off vs manual.** Default is off-at-zero-viewers. Does the UI need a "keep
   this profile hot" toggle for a profile the operator always wants ready?
2. **Viewer counting.** The simplest source is the RTSP server's client count per
   mount. Needs verification that `GstRtspServer` reports it reliably per mount.
3. **Catalog source** - compiled-in defaults, an INI/JSON file, or generated units.
   File-based is friendlier for embedded provisioning.
4. **Queue sizing per branch.** Default is 200 buffers. With `x264enc` on a slow
   branch the queue could grow; needs a leaky-queue policy decision under load.
5. **Crash isolation.** Accepted as shared-fate for now (section 4.2). Revisit only
   if a profile crash in the field justifies the capture-process refactor.

## 10. What this design deliberately does not do

- No process-per-profile. Measured CPU saving comes from the valve, not from process
  isolation.
- No pad probes or pad blocking to disable a branch - section 3.4 explains why that
  stalls the whole pipeline.
- No shared-memory frame fan-out across processes (`interpipe`), not until measured
  need.
- No WebRTC in the media path until Phase 7.
- No auth, TLS, or multi-host exposure - localhost only for now.
- No Qt in the server - it must stay headless for systemd and for the Pi.
