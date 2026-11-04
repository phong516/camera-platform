# GStreamer Resources

## Knowledge

- [Official docs: tee](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html)
  Branching element reference. Use for: tee properties (`allow-not-linked`), the "queue per branch" warning, input/output-selector notes.
- [Official docs: queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html)
  Thread-boundary element reference. Use for: `max-size-*` limits (200 buffers / 10 MB / 1 s), `leaky` modes, default blocking behaviour.
- [Official docs: valve](https://gstreamer.freedesktop.org/documentation/coreelements/valve.html)
  Runtime on/off switch reference. Use for: `drop`, `drop-mode` (`drop-all`, `forward-sticky-events`, `transform-to-gap`), event-handling semantics.
- [Design doc: Preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html)
  The authoritative explanation of *why* sinks block until they receive a buffer/GAP/EOS. Use for: diagnosing "pipeline never starts" stalls — the single most valuable doc found so far.
- [Official docs: GstRtspServer](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/index.html) and [GstRtspMediaFactory](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-media-factory.html)
  RTSP serving layer (Phase 2). Use for: mounts, shared media, `media-configure` bridge. Covered in Lessons 5, 6, 8, and 10.
- [GLib: The Main Event Loop](https://docs.gtk.org/glib/main-loop.html)
  Authoritative description of contexts, sources, and (critically) "Memory management of sources". Use for: source lifetime, `g_source_remove()`, running a context from one thread, integrating an external event loop.
- [GLib: GMainLoop API](https://docs.gtk.org/glib/struct.MainLoop.html) and [GMainContext API](https://docs.gtk.org/glib/struct.MainContext.html)
  Ref/unref/run/quit contracts and context semantics. Use for: "why does this need unref?", thread-default contexts, private vs default context.
- [GLib source: gmain.c](https://gitlab.gnome.org/GNOME/glib/-/blob/main/glib/gmain.c)
  Implementation truth for ownership questions. Use for: `g_main_loop_new` refs the context, `g_main_loop_run` holds a temporary ref, unref frees at zero.
- [GStreamer: appsink signals](https://gstreamer.freedesktop.org/documentation/app/appsink.html)
  Signal-level reference. Use for: which thread emits `new-sample` / `eos` (the streaming thread), `emit-signals`, `sync`, and queue limits.
- [Official docs: appsrc](https://gstreamer.freedesktop.org/documentation/app/appsrc.html)
  Feeding an app-driven pipeline (the encoder→RTSP bridge). Use for: `is-live`, `block`, timestamp/caps handling in bridge code.
- [Qt: MediaPlayer QML Type](https://doc.qt.io/qt-6/qml-qtmultimedia-mediaplayer.html)
  Qt's high-level playback object. Use for: setting an RTSP source, starting playback, and inspecting playback errors and state.
- [Qt: VideoOutput QML Type](https://doc.qt.io/qt-6/qml-qtmultimedia-videooutput.html)
  QML scene-graph video presentation. Use for: attaching a `MediaPlayer` and choosing fit/crop behavior.
- [Qt: Qt Multimedia overview](https://doc.qt.io/qt-6/qtmultimedia-index.html)
  Module setup and platform/backend overview. Use for: adding `Qt6::Multimedia` and checking what media backend the target platform uses.
- [Qt: GStreamer backend for Qt Multimedia](https://doc.qt.io/qt-6/qtmultimedia-gstreamer.html)
  Embedded Linux backend notes, limitations, and customization points. Use for: investigating target-specific performance or backend behavior; not a promise that every RTSP source works.
- [GStreamer: qml6glsink](https://gstreamer.freedesktop.org/documentation/qml6/qml6glsink.html)
  Direct GStreamer-to-Qt Quick scene-graph sink. Use for: the alternative when direct pipeline control is needed, especially its OpenGL display/context ordering constraints.

## Wisdom (Communities)

- [GStreamer Discourse](https://discourse.gstreamer.org/)
  Official forum, active maintainers (esp. the App Development category). Use for: any real-world pipeline question; search it before posting — most "tee deadlock" style problems already have answers.
- [gstreamer-devel mailing list archive](https://lists.freedesktop.org/archives/gstreamer-devel/)
  Historical archive where core devs answer design questions. Use for: slower, deeper questions; browse threads on preroll/tee/latency before posting.

## Gaps

- No high-trust book or structured video course found yet — everything rests on official docs + Discourse. Acceptable for now; revisit if concepts feel disconnected.
- No verified local (Vietnam) community or meetup surfaced. The user has not yet stated a preference on joining communities.
- No high-trust source yet on integrating a `GMainContext` with Qt's event loop (GSource vs dedicated thread). Find one before the Qt integration lesson; GLib's Main Contexts tutorial is the starting point.
- Qt documents the playback and rendering APIs, but RTSP/codec support varies with Qt version, media backend, and target build. Validate the actual stream on the deployment target before committing to the playback path.
