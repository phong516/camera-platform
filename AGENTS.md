# AGENTS.md

Notes for AI agents working in this repository. Read this before editing code.

## What this repository is

`camera-platform` is both a C++/GStreamer/Qt project and a **teaching workspace**.

- The learner (Phong) writes the project code himself. The agent's role here is
  teaching and review, not implementing `server/`, `web/`, or `video/` unless he
  explicitly asks. See `NOTES.md` (learner preferences) and `MISSION.md`.
- Teaching material is a first-class part of the repo and is tracked in git:
  `lessons/*.html`, `labs/*.cpp`, `reference/*.html`, `learning-records/*.md`,
  `assets/lesson.css`, `assets/quiz.js`.
- Lessons follow a fixed format: short concept, then a runnable experiment with
  real measured numbers, then one concrete step in the repo.
- `NOTES.md` is the running lab notebook: verified GStreamer measurements,
  retractions, harness caveats. Treat it as authoritative measured history.
- `docs/ARCHITECTURE-SERVER-CLIENT.md` is the architecture specification. The
  source tree is the truth where docs have drifted (see "Documentation drift").

## Quick commands

```bash
# Configure + build (Qt client built automatically when Qt6 >= 6.10.2 is found)
cmake -S . -B build
cmake --build build -j"$(nproc)"

# Server only (headless/embedded; this is what CI configures)
cmake -S . -B build -DCAMERA_PLATFORM_WITH_QT=OFF
cmake --build build -j"$(nproc)"

# Tests
ctest --test-dir build --output-on-failure

# Run
./build/server/camera-server        # headless; RTSP on :8554, no Qt
./build/client-qt/camera-client-qt  # Qt/QML client
```

- CI (`.github/workflows/ci.yml`) uses Ubuntu 26.04, Ninja, `RelWithDebInfo`,
  `-DCAMERA_PLATFORM_WITH_QT=OFF`, and the same build/test commands.
- CI installs `libgstreamer1.0-dev`, `libgstreamer-plugins-base1.0-dev`,
  `libgstrtspserver-1.0-dev`, `gstreamer1.0-plugins-base`, and
  `gstreamer1.0-plugins-ugly`. The last one is required: `ProfileBin` creates
  `x264enc` at runtime and every profile branch fails if it is missing.
- Local toolchain: C++23 (`CMAKE_CXX_STANDARD 23`), g++ / CMake, GStreamer
  1.28.2 on WSL2. `CMAKE_EXPORT_COMPILE_COMMANDS ON` keeps
  `build/compile_commands.json` current for clangd/cpptools.

## Layout

| Path | Purpose |
| --- | --- |
| `common/` | Header-only contract: ports, API paths, mount names, state names (`Protocol.hpp`). No GStreamer, no Qt. |
| `model/` | Header-only value types: `ProfileInfo`, `CameraState`, `SystemStatus`, `JsonUtil.hpp` (`jsonEscape`). No GStreamer, no Qt. `toJson()` functions are inline. |
| `web/` | Control plane: HTTP parsing, `WebServer`, `ApiController`, and the `CameraControl` seam. POSIX sockets, no external HTTP library. |
| `video/` | GStreamer capture pipeline and source/sink seams (`VideoSource`, `VideoSink`). |
| `server/` | Headless streaming server: `Application` (lifecycle), `ProfileHost` (tee + profile bins), `ProfileBin` (one encoded branch), `RtspProfileServer` (RTSP bridge). Never links Qt. |
| `client-qt/` | Qt/QML client. Talks HTTP for control and RTSP for video. Never links GStreamer. |
| `tools/` | Placeholder INTERFACE target for a future CLI. |
| `tests/` | No test framework; see `tests/TestSupport.hpp`. |
| `labs/` | Teaching experiments, **not part of the CMake build**. Built manually with `g++`/`gcc` + `pkg-config`. |
| `cmake/FindGStreamer.cmake` | pkg-config finder; requires `gstreamer-1.0`, `gstreamer-base-1.0`, `gstreamer-app-1.0`, `gstreamer-rtsp-server-1.0`. |

Dependency rule: `common/` and `model/` are dependency-free; `web/` depends on
`model`; `video/` depends only on GStreamer; `server/` depends on all of them;
`client-qt/` depends on Qt plus `common`/`model`, never on `server` or GStreamer.

## Architecture and control flow

One `GstPipeline` holds the capture and every profile:

```text
videotestsrc/v4l2src -> videoconvert -> capsfilter -> tee
  tee -> ProfileBin(hd):    queue -> valve -> videoscale -> capsfilter -> x264enc -> appsink
  tee -> ProfileBin(sd):    ... same chain ...
  tee -> ProfileBin(low):   ...
  tee -> ProfileBin(thumb): ...
```

- `ProfileHost` owns the source, tee, and all `ProfileBin`s. It keeps one
  pipeline state for everything.
- `ProfileBin::setDrop()` flips the valve. A closed valve discards buffers so the
  encoder never runs; this is the per-profile on/off switch.
- `RtspProfileServer` creates one shared RTSP media factory per profile
  (`shared=TRUE`, one mount per profile, port `proto::kRtspPort = 8554`). Each
  factory's launch string is `appsrc ! h264parse ! rtph264pay`. On
  `media-configure` it finds `profile_src` inside the media and connects the
  branch's borrowed appsink `new-sample` signal to it. On `media-unprepared` it
  drops the appsrc reference.
- `CameraControl` (`web/CameraControl.hpp`) is the seam between the HTTP layer
  and `server/Application`: `systemStatus()`, `profiles()`,
  `setProfileEnabled(name, enabled)`. Tests and `ApiController` use this seam
  rather than concrete server classes.

Startup order in `Application::initialize()` matters:

1. `gst_init()`
2. create `GMainContext` and `GMainLoop`
3. install signal sources on that context (not the default context)
4. set the video source and build the pipeline
5. construct `RtspProfileServer` **after** `setupPipeline()` (it binds the
   appsinks that only exist then)
6. attach the pipeline bus watch
7. attach the RTSP server source to the same context

Shutdown: `gst_deinit()` is deliberately not called from `~Application` (the
destructor body runs before member teardown, which still touches Gst objects).
The current `main.cpp` does not call it either; cleanup is left to process exit.
If you add it, call it only after the `Application` object is fully destroyed.

## Conventions

- Includes are repo-root-relative in cross-directory code: `"server/ProfileHost.hpp"`,
  `"model/Profile.hpp"`, `"common/Protocol.hpp"`. Each target adds
  `${PROJECT_SOURCE_DIR}` explicitly; there is no global `include_directories()`.
  Files inside `server/` also use local includes such as `"ProfileHost.hpp"`.
- Public headers use `#pragma once`.
- Classes are PascalCase, methods and locals camelCase, private members `m_`,
  constants `kCamelCase` (e.g. `proto::kStateLive`, `kDefaultCatalog`).
- Operation results use `PipelineOutcome { bool ok; std::string error; }`.
  Return a reason on failure; do not log from library code or use sentinel error
  strings.
- State strings on the wire are the constants in `common/Protocol.hpp`
  (`off`, `ready`, `live`, `error`). `ProfileInfo::state` stores the raw string
  deliberately; do not introduce a second enum.
- JSON serialization is header-only in `model/*.hpp` and uses `jsonEscape`.
  Models are plain values; only `client-qt/qml/CameraBackend` is a `QObject`.
- Tests use `CHECK`/`CHECK_EQ` from `tests/TestSupport.hpp`, each test file has
  its own `int main()`, and ends with `TEST_RETURN()`.

## GStreamer and lifecycle gotchas

- **Pipeline state is global.** `gst_element_set_state()` on a branch returns
  success but changes nothing, and `gst_element_get_state()` on a child reports
  the parent's state. Never implement profile on/off with state; use the valve.
- **Never start with a valve closed unless the sink sets `async=false`.** A
  closed valve starves the branch sink during preroll and can stall the whole
  pipeline. `ProfileBin::createElement()` creates its valve open. Note the
  current startup bug: `Application`'s constructor calls
  `setProfileEnabled(..., false)` before the pipeline exists, so the valve calls
  are no-ops and every profile reports `off` while its valve is physically open.
  Applying `setProfileEnabled()` after `setupPipeline()` (or fixing the ordering)
  is required for real lazy startup. Mid-run open/close is safe.
- **Do not disable a branch with a pad probe or pad block.** The tee pushes
  synchronously; one stalled pad stalls every profile.
- **The RTSP bridge restamps every buffer.** `RtspProfileServer::onNewSample()`
  copies the buffer, sets PTS/DTS/duration to `GST_CLOCK_TIME_NONE`, and pushes
  it to appsrc. Passing producer timestamps through made clients receive zero
  samples with no error (measured in `NOTES.md`).
- `RtspProfileServer` must be constructed after the host pipeline is set up and
  must be destroyed/detached before the host: it borrows `GstAppSink`s from
  `ProfileHost`. Mounts hold raw `GstAppSrc`/`GstAppSink` pointers.
- GStreamer ownership: `gst_element_factory_make()` and `gst_bin_new()` return
  floating refs; `gst_bin_add*()` consumes them, so do not unref after adding.
  `gst_element_get_static_pad()` and `gst_element_request_pad_simple()` return
  full refs that must be unreffed. Use `gst_element_no_more_pads()` on static
  bins.
- `ProfileHost::stop()` moves the pipeline to PAUSED; full teardown to NULL and
  element disposal happens in `cleanup()` from the destructor.
- Current `Application::stop()` stops the host and unrefs the main loop;
  RTSP detach happens in `~RtspProfileServer`, although the header contract says
  `stop()` should detach first. Be careful if you change teardown order.
- Concurrency contract: main loop thread handles bus/RTSP/signal sources; the
  web thread (once Phase 3 wires it) calls `CameraControl`. `Application` guards
  host/state access with `m_stateMutex`; never hold that mutex across a blocking
  `set_state`/`get_state`. `g_main_loop_quit()` is the one cross-thread safe
  entry point.
- `g_unix_signal_add()` attaches to the default context. `Application` builds
  `GSource`s by hand and attaches them to its own context; sources on a non-default
  context must be destroyed by pointer, not `g_source_remove(id)`.

## Web layer status

- `web/WebServer.cpp` is a work in progress (currently `start()` creates a
  socket and thread; `run()` is empty). The class comment in
  `web/WebServer.hpp` is the authoritative completion contract: route
  registration before `start()`, 8 KiB request cap, 5 s receive timeout, one
  request per connection, 400/404/405 behavior, idempotent `stop()`.
- `web/ApiController.cpp` is empty. Its header is the implementation contract.
  `tests/web/test_apicontroller.cpp` exists but is intentionally not compiled;
  uncomment its block in `tests/CMakeLists.txt` once the implementation lands.
- The HTTP API contract is in `common/Protocol.hpp` and section 6 of
  `docs/ARCHITECTURE-SERVER-CLIENT.md`. `on`/`off` are idempotent, errors are
  reported as profile data (`state == "error"` + `lastError`), not HTTP errors.

## Testing

- Six registered tests: `web.http`, `video.videopipeline`, `server.profilehost`,
  `server.rtspprofileserver`, `server.profilebin`, `server.application`.
- Tests link the real libraries and use `gst_init()`. The video test uses
  `tests/video/FakeSink.hpp` (`fakesink`) so it runs headless; no display is
  needed for the server/tests.
- `test_rtspprofileserver` uses port `0` to avoid collisions.
- Test names are registered via `add_camera_test(target, name, source, library)`
  in `tests/CMakeLists.txt`. Add new tests through that helper.
- Running the server directly is headless-safe: it streams `videotestsrc`
  through the profile branches and does not open a window.

## Labs and lessons

- `labs/` is deliberately outside CMake. Build a lab manually, e.g.:

  ```bash
  g++ -Wall -O2 -o ~/cam-recon/lab6 labs/lab6-rtsp-bridge.cpp \
      $(pkg-config --cflags --libs gstreamer-rtsp-server-1.0 gstreamer-rtsp-1.0 \
          gstreamer-1.0 gstreamer-app-1.0)
  ```

  `labs/compile_flags.txt` exists so clangd resolves GStreamer types for labs.
- Lab output lives in `~/cam-recon/`, not `/tmp`: `/tmp` is a ~1.5 GB tmpfs and
  the WSL VM can wipe it. Always bound experiments with `num-buffers`/`timeout`
  and clean up large files.
- Headless labs must use `fakesink`/`filesink`; real-camera (`v4l2src`) work
  cannot be verified in WSL (no `/dev/video`).
- Lessons are numbered HTML files in `lessons/` and use `../assets/lesson.css`
  and `../assets/quiz.js`. `reference/` holds long-form references.
  `learning-records/` records teaching decisions and inferred learner knowledge;
  read the relevant record before teaching a topic again.
- `NOTES.md` contains measured lab facts, including retractions. Do not re-derive
  or contradict them without a new measurement.

## Documentation drift

The source tree has moved past some prose docs; trust code over comments in these
cases:

- `README.md` still describes the pre-split single-process design and says
  `CameraView.qml`/`Settings.qml` are not in the QML module. All four QML files
  are in `client-qt/qml/CMakeLists.txt` now.
- The server no longer uses `autovideosink`; `ProfileHost` ends every branch in
  `appsink` for the RTSP bridge.
- `client-qt/main.cpp` currently comments out `CameraBackend` instantiation and
  the `backend` context property; `CameraBackend.cpp` is a stub even though the
  header defines the full QML contract and the QML files bind to `backend`.
- `video/VideoPipeline` is still tested and buildable but is no longer the
  server path; `ProfileHost` replaced it.
