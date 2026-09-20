#pragma once

#include <cstdint>
#include <sstream>
#include <string>

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

/// Escape a UTF-8 string so it can be embedded inside a JSON string literal.
/// HEADER-ONLY: do not define in .cpp.
inline std::string jsonEscape(const std::string &value)
{
    std::string out;
    out.reserve(value.size() + 8);

    for (const char c : value)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                static const char *hexDigits = "0123456789abcdef";
                const auto byte = static_cast<unsigned char>(c);
                out += "\\u00";
                out += hexDigits[(byte >> 4) & 0x0F];
                out += hexDigits[byte & 0x0F];
            }
            else
            {
                out += c;
            }
            break;
        }
    }

    return out;
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
