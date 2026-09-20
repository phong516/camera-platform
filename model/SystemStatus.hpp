#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "model/CameraState.hpp" // jsonEscape

/// How the device is currently attached to the network.
enum class NetworkState
{
    Unknown,
    Down,
    Wired,
    Wireless
};

inline const char *toString(NetworkState state)
{
    switch (state)
    {
    case NetworkState::Unknown:
        return "unknown";
    case NetworkState::Down:
        return "down";
    case NetworkState::Wired:
        return "wired";
    case NetworkState::Wireless:
        return "wireless";
    }
    return "unknown";
}

/// Host-level measurements reported to the UI and the web API.
/// Plain value type (no Qt). Populated by Application / NetworkManager.
struct SystemStatus
{
    NetworkState network{NetworkState::Unknown};
    std::string ipAddress;
    double cpuPercent{0.0};
    double memoryPercent{0.0};
    double bitrateKbps{0.0};
    std::uint64_t droppedFrames{0};
    bool cameraConnected{false};
    std::int64_t uptimeMs{0};
};

/// HEADER-ONLY: do not define in .cpp.
inline std::string toJson(const SystemStatus &status)
{
    std::ostringstream out;
    out << '{'
        << "\"network\":\"" << toString(status.network) << "\","
        << "\"ipAddress\":\"" << jsonEscape(status.ipAddress) << "\","
        << "\"cpuPercent\":" << status.cpuPercent << ","
        << "\"memoryPercent\":" << status.memoryPercent << ","
        << "\"bitrateKbps\":" << status.bitrateKbps << ","
        << "\"droppedFrames\":" << status.droppedFrames << ","
        << "\"cameraConnected\":" << (status.cameraConnected ? "true" : "false") << ","
        << "\"uptimeMs\":" << status.uptimeMs
        << '}';
    return out.str();
}
