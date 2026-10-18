#pragma once

#include <string>

/// Escape a UTF-8 string so it can be embedded inside a JSON string literal.
/// Shared serialization helper, living next to the value types it serves -
/// both the model's toJson() and the web layer include it from here, so no
/// header defines a second escaping routine. HEADER-ONLY: do not define in .cpp.
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
