#pragma once

#include <cstdint>
#include <string>

/// The control-plane and media-plane contract, shared by the server and every
/// client. Values live here so a port or path change cannot drift between the
/// server, the Qt client, and the browser page.
///
/// Header-only, no GStreamer, no Qt.
namespace proto
{

// --- Control plane: HTTP/JSON -------------------------------------------------

/// Default port for camera-server's REST API and the static control page.
inline constexpr std::uint16_t kControlPort = 8080;

/// Default mount point for the media plane.
inline constexpr std::uint16_t kRtspPort = 8554;

/// RTSP is the internal protocol. A browser cannot play it; the browser client
/// gets a WebRTC bridge in Phase 7 (docs/ARCHITECTURE-SERVER-CLIENT.md).
inline constexpr const char *kRtspScheme = "rtsp";

/// Base used by the server and by clients that only know the API port. Clients
/// should prefer the absolute `url` field from /api/profiles over building one.
/// `host` defaults to loopback for local development; a LAN deployment passes
/// the board's address instead.
inline std::string rtspUrlFor(std::uint16_t port, const std::string &mount,
                              const std::string &host = "127.0.0.1")
{
    return std::string(kRtspScheme) + "://" + host + ":" + std::to_string(port) + "/" + mount;
}

// --- REST API paths -----------------------------------------------------------

/// The control-plane contract lives entirely here (docs/ARCHITECTURE-SERVER-CLIENT.md
/// section 6). Profile-scoped routes append the name, so the literal keys the
/// server registers are built with profileActionPath():
///
///   GET  /api/health                 -> 200 {"ok":true}
///   GET  /api/status                 -> 200, toJson(SystemStatus)
///   GET  /api/profiles               -> 200 {"profiles":[ toJson(ProfileInfo)... ]}
///   GET  /api/profiles/{name}        -> 200, toJson(ProfileInfo)  | 404 unknown
///   POST /api/profiles/{name}/on     -> 200, toJson(ProfileInfo)  | 404 unknown
///   POST /api/profiles/{name}/off    -> same; both idempotent
///
/// on/off are idempotent: 200 whether the valve changed anything or not.
/// Errors are data, not HTTP failures: a broken profile reports
/// state==proto::kStateError plus lastError in its ProfileInfo.
inline constexpr const char *kApiProfiles = "/api/profiles";  ///< GET, catalog root
inline constexpr const char *kApiStatus   = "/api/status";    ///< GET - host + supervisor health
inline constexpr const char *kApiHealth   = "/api/health";    ///< GET - liveness probe
inline constexpr const char *kActionOn  = "/on";   ///< appended to /api/profiles/{name}
inline constexpr const char *kActionOff = "/off";  ///< appended to /api/profiles/{name}

/// Builds the route key for a profile action, e.g. "/api/profiles/hd/on".
/// The one place POST paths are concatenated, for server and clients alike.
inline std::string profileActionPath(const std::string &name, const char *action)
{
    return std::string(kApiProfiles) + "/" + name + action;
}

// --- Profile state names ------------------------------------------------------

/// Wire values for a profile's state, as reported in /api/profiles.
///   off   - valve closed, no encoder work, ~0 CPU
///   ready - valve open, nobody watching (encodes and discards - avoid)
///   live  - valve open, at least one RTSP viewer
///   error - pipeline problem; see lastError
///
/// There is deliberately no "paused": all profiles share one pipeline, and a
/// GStreamer pipeline has a single global state. See section 3.1 of
/// docs/ARCHITECTURE-SERVER-CLIENT.md.
inline constexpr const char *kStateOff   = "off";
inline constexpr const char *kStateReady = "ready";
inline constexpr const char *kStateLive  = "live";
inline constexpr const char *kStateError = "error";

} // namespace proto
