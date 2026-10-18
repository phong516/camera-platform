#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "common/Protocol.hpp" // proto::kState* - wire values for `state`
#include "model/JsonUtil.hpp"  // jsonEscape

/// One profile in the server's catalog: a named encoding branch (resolution,
/// fps, bitrate) riding the shared capture pipeline, plus its live state.
/// Plain value type (no Qt, no Gst). This struct is both the server's storage
/// shape and the JSON shape clients parse from GET /api/profiles - the field
/// names below are the wire names (ARCHITECTURE-SERVER-CLIENT.md section 6).
///
/// `state` deliberately carries the raw wire value from Protocol.hpp
/// (proto::kStateOff / kStateReady / kStateLive / kStateError) instead of an
/// enum: the constants in common/Protocol.hpp are the single source of truth,
/// and a second enum would be one more thing to keep in sync with the wire.
///
/// SERVER-OWNED STATE: clients only ever read these values; on/off requests
/// ask the server to change them (see the routes in common/Protocol.hpp).
struct ProfileInfo
{
    std::string name;                    ///< catalog key, e.g. "hd"
    std::uint32_t width{0};              ///< branch output resolution
    std::uint32_t height{0};
    std::uint32_t fps{30};               ///< denominator fixed at 1 by the catalog
    std::uint32_t bitrateKbps{0};
    std::string state{proto::kStateOff}; ///< wire value from Protocol.hpp
    std::string url;                     ///< absolute rtsp URL, from proto::rtspUrlFor
    std::uint32_t viewers{0};            ///< RTSP clients attached to the mount
    double cpuPercent{0.0};
    std::string lastError;               ///< non-empty only while state == error
};

/// HEADER-ONLY: do not define in .cpp.
inline std::string toJson(const ProfileInfo &info)
{
    std::ostringstream out;
    out << '{'
        << "\"name\":\"" << jsonEscape(info.name) << "\","
        << "\"width\":" << info.width << ","
        << "\"height\":" << info.height << ","
        << "\"fps\":" << info.fps << ","
        << "\"bitrateKbps\":" << info.bitrateKbps << ","
        << "\"state\":\"" << jsonEscape(info.state) << "\","
        << "\"url\":\"" << jsonEscape(info.url) << "\","
        << "\"viewers\":" << info.viewers << ","
        << "\"cpuPercent\":" << info.cpuPercent << ","
        << "\"lastError\":\"" << jsonEscape(info.lastError) << "\""
        << '}';
    return out.str();
}
