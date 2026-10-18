#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "model/JsonUtil.hpp" // jsonEscape

/// Lifecycle of the capture pipeline as seen by the rest of the application.
enum class CameraStatus
{
    Idle,
    Starting,
    Streaming,
    Paused,
    Error
};

inline const char *toString(CameraStatus status)
{
    switch (status)
    {
    case CameraStatus::Idle:
        return "idle";
    case CameraStatus::Starting:
        return "starting";
    case CameraStatus::Streaming:
        return "streaming";
    case CameraStatus::Paused:
        return "paused";
    case CameraStatus::Error:
        return "error";
    }
    return "unknown";
}

/// Snapshot of the capture configuration and health.
/// Deliberately a plain value type (no Qt) so the model layer can be reused
/// before the QML presentation layer exists. Phase 5 will wrap it in a QObject.
struct CameraState
{
    CameraStatus status{CameraStatus::Idle};
    std::string sourceName{"videotestsrc"};
    std::uint32_t width{640};
    std::uint32_t height{480};
    std::uint32_t fpsNum{30};
    std::uint32_t fpsDenom{1};
    std::string pixelFormat{"RGB"};
    std::int64_t uptimeMs{0};
    std::string lastError;
};

/// HEADER-ONLY: do not define in .cpp.
inline std::string toJson(const CameraState &state)
{
    std::ostringstream out;
    out << '{'
        << "\"status\":\"" << toString(state.status) << "\","
        << "\"sourceName\":\"" << jsonEscape(state.sourceName) << "\","
        << "\"width\":" << state.width << ","
        << "\"height\":" << state.height << ","
        << "\"fpsNum\":" << state.fpsNum << ","
        << "\"fpsDenom\":" << state.fpsDenom << ","
        << "\"pixelFormat\":\"" << jsonEscape(state.pixelFormat) << "\","
        << "\"uptimeMs\":" << state.uptimeMs << ","
        << "\"lastError\":\"" << jsonEscape(state.lastError) << "\""
        << '}';
    return out.str();
}
