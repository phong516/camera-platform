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
inline std::string rtspUrlFor(std::uint16_t port, const std::string &mount)
{
    return std::string(kRtspScheme) + "://127.0.0.1:" + std::to_string(port) + "/" + mount;
}

// --- REST API paths -----------------------------------------------------------

inline constexpr const char *kApiProfiles = "/api/profiles";  ///< GET
inline constexpr const char *kApiStatus   = "/api/status";    ///< GET
inline constexpr const char *kApiHealth   = "/api/health";    ///< GET

/// POST /api/profiles/{name}/on  - open the valve (idempotent)
/// POST /api/profiles/{name}/off - close the valve (idempotent)
inline constexpr const char *kActionOn  = "/on";
inline constexpr const char *kActionOff = "/off";

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
