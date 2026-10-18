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
  RTSP serving layer (Phase 2). Use for: mounts, shared media, `media-configure` bridge. (Lessons for this not yet written.)
- [Official docs: appsrc](https://gstreamer.freedesktop.org/documentation/app/appsrc.html)
  Feeding an app-driven pipeline (the encoder→RTSP bridge). Use for: `is-live`, `block`, timestamp/caps handling in bridge code.

## Wisdom (Communities)

- [GStreamer Discourse](https://discourse.gstreamer.org/)
  Official forum, active maintainers (esp. the App Development category). Use for: any real-world pipeline question; search it before posting — most "tee deadlock" style problems already have answers.
- [gstreamer-devel mailing list archive](https://lists.freedesktop.org/archives/gstreamer-devel/)
  Historical archive where core devs answer design questions. Use for: slower, deeper questions; browse threads on preroll/tee/latency before posting.

## Gaps

- No high-trust book or structured video course found yet — everything rests on official docs + Discourse. Acceptable for now; revisit if concepts feel disconnected.
- No verified local (Vietnam) community or meetup surfaced. The user has not yet stated a preference on joining communities.
