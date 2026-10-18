# Notes

## Learner preferences
- Direct questions get direct answers. Phong will say "stop, I just asked..." if you run ahead into implementation. Answer first; propose actions after.
- Phong implements all `camera-platform` code himself. The agent's job here is teaching, not writing project code.
- Chosen lesson format: **short concept → runnable experiment with real measured numbers → one concrete step in the repo**.
- Prefer terse, factual language. Show real command output, not paraphrases.

## Environment notes
- Workspace = repo root of `camera-platform` (teaching files live alongside code, untracked in git for now — decide later whether to commit or ignore).
- WSL2 Ubuntu, GStreamer 1.28.2, headless. `gst-launch-1.0` labs must use `fakesink`/`filesink`.
- `/tmp` is a 1.5 GB tmpfs: always bound experiments with `num-buffers` and/or `timeout`, and clean up `*.raw`/`*.264` files. (Filled it once already with an unbounded 720p test.)
- Windows access via `\\wsl.localhost\Ubuntu\...`; run commands with `wsl.exe -d Ubuntu -- bash ...`. Avoid PowerShell quoting pitfalls: write scripts to files, don't inline complex ones.
- `identity sleep-time=<µs>` is the standard trick for simulating a slow consumer in labs.

## Verified lab facts (measured 2026-10, keep reusing)
- tee + 2 branches with queues, 60 frames of 320x240 I420: each branch file = 6,912,000 B (= 60 × 115,200).
- tee + 2 branches with NO queues: deadlocks at PREROLLING, 0 bytes, forever. Queue on the first branch alone unblocks it, but the second branch then throttles the first.
- Queue grants ~1 s of slack (its `max-size-time` default) before the slowest branch sets the pace: fast 39 frames vs slow 7 frames in a 4 s window.
- Valve `drop=true` at startup (default `drop-mode=drop-all`) starves that branch's sink → **nothing in the pipeline plays**; a live pipeline still prints "PREROLLED/PLAYING" yet writes 0 bytes everywhere. This is the preroll gate, not a bug.
- `drop-mode=transform-to-gap` rescues it only if no encoder sits between valve and sink (`videoscale`/`capsfilter` pass gaps; `x264enc` does not).
- `<sink> async=false` on the starved sink works for `fakesink` (CPU 182% both-open → 94.6% dropped, vs 92.3% single-branch baseline); not reliable for `filesink`+encoder path.
- GStreamer 1.28 C++ API facts (verified against installed headers): `gst_element_add_ghost_pad` is gone — use `gst_ghost_pad_new(name, target)` (floating; refs the target) + `gst_element_add_pad(bin, ghost)`; `gst_element_get_static_pad` and `gst_element_request_pad_simple` (gstelement.h:1273, replaces deprecated `gst_element_get_request_pad`) return full refs you must unref; call `gst_element_no_more_pads(bin)` on static bins; `gst_bin_get_by_name` lookups are scoped to the bin (siblings may reuse internal names).
- lab3 (bin + ghost pad + runtime drop, 720p@2500k live, filesink): file grows to 983,326 B by t=3 s; setting `pvalve.drop=true` at t=3 freezes it byte-exact (same value at t=5 and t=6) — no in-flight leakage.
- lab4 (two profile bins, hd 1280x720@2500k / sd 854x480@1200k): t=3s hd=1,012,757 sd=481,104 (drop sd) → t=5s hd=1,575,868 sd=481,104 (frozen) → t=6s hd=1,939,738 sd=481,104 (resume) → t=9s hd=2,856,054 sd=885,522. Sister branch undisturbed throughout; resume is clean (≈135 KB/s, no backlog burst — dropped buffers die in the valve, queue stays empty). Mid-run drop = silence for that branch's consumer (parked UX decision for RTSP).
- lab-async-appsink (startup-closed valve on the real sink type, 320x240): (1) valve closed + `appsink async=false` → pipeline plays; open sibling branch byte-identical to reference (1,353,990 B); reopen at t=2 s → first sample 65 ms later, 119 samples in 4 s (30 fps). (2) valve closed + default `async=true` → permanent freeze; reopening the valve later does NOT rescue it (0 B / 0 samples forever). (3) earlier filesink+encoder failure stands — `async=false` is sink-specific. Consequence for the repo's lazy design (valve closed until first viewer): ONLY viable with `async=false` set on the profile appsink; never with the default. RTSP-side (appsrc/media pipeline) still to verify in the RTSP lesson.
- Stall mechanics (probes: labs/lab-preroll-states.cpp + labs/lab-stall-mechanism.cpp): live + valve closed at startup + default-async sink → pipeline parks at `state=PAUSED pending=PLAYING` (PAUSED commits via live NO_PREROLL), the starved sink sits at `READY pending=PAUSED`, upstream elements commit PLAYING; non-live → pipeline `READY pending=PLAYING`. Flow wedges after ~1 s: the stopped branch (its sink parked in PAUSED, not rendering) → its queue hits `max-size-time=1 s` (≈30 buffers at 30fps) → tee's sequential push blocks → the SOURCE's thread freezes → every other branch starves (qB recv stops at 31). Reopen does nothing: no buffers ever arrive at the valve again. Single-branch variant (no wedged sibling): reopen DOES rescue (~30 fps samples from reopen on). fakesink freezes identically to filesink → not sink-specific. Bus during the stall: zero messages — no ERROR/WARNING/ASYNC_DONE, silence after startup; detection requires a `gst_element_get_state` timeout watchdog.
- Design consequence for Phase 2 (`--drop <name>`): never start with a valve closed; drop only after preroll completed. Mid-run drops are safe because queues decouple branches (now measured end-to-end in C++: other branches unaffected, dropped branch freezes byte-exact, resume clean).
- CPU reference (720p30 is-live, x264enc ultrafast+zerolatency → filesink): one branch ≈ 92%, two branches ≈ 182%.

## Working notes
- Startup sequence for Phase 2 (decided after the stall probes): build all profile bins with valves OPEN → set pipeline PLAYING → wait for the PLAYING commit via `gst_element_get_state` watchdog (not a fixed sleep) → only then apply `--drop` / close viewer-less profiles. Never drop before PLAYING commits (that is the permanent stall). Mid-run drop/resume is proven safe (lab4).
- Lesson arc: 0001 tee+queue (done), 0002 valve+preroll gate (done), 0003 bins & ghost pads (done), 0004 ProfileHost / runtime control (done); next: RTSP media factories + appsink swap → appsrc bridge → `--drop` integration.
- Teaching labs now live in the repo: `labs/lab3-profile-bin.cpp`, `labs/lab4-profile-host.cpp`, `labs/lab-async-appsink.cpp` (untracked, like the rest of the teaching files — commit-or-ignore still undecided). Built binaries + outputs in `/tmp/cam-recon/` (may not survive reboot; numbers are recorded in this file and in the lessons).
- User's next actions (their work, not the agent's): implement `server/ProfileBin.hpp/.cpp`, then `server/ProfileHost.hpp/.cpp`, smoke-run both; teacher agent reviews before any `Application` integration or `--drop` CLI work.
- User's repo has known build-break (`client-qt/qml/CameraBackend.cpp` ctor mismatch) and a `getDuration()` UB — flagged separately, not part of the curriculum; re-raise when they pick up repo work.
