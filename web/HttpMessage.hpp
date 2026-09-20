#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>

#include "model/CameraState.hpp" // jsonEscape

/// HEADER-ONLY: do not define in .cpp.
inline std::string toLowerHelper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return value;
}

/// HEADER-ONLY: do not define in .cpp.
inline std::string trimHelper(const std::string &value)
{
    const auto isSpace = [](unsigned char c)
    { return std::isspace(c) != 0; };
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && isSpace(static_cast<unsigned char>(value[begin])))
        ++begin;
    while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(begin, end - begin);
}

/// A parsed HTTP/1.1 request. Bodies are limited by the caller (WebServer caps
/// a single request at 8 KiB); this type does no allocation limits of its own.
struct HttpRequest
{
    std::string method;  ///< upper-case as sent, e.g. "GET"
    std::string path;    ///< without the query string, e.g. "/api/status"
    std::string body;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers; ///< keys lower-cased

    /// Case-insensitive lookup. Returns an empty string when absent.
    /// HEADER-ONLY: do not define in .cpp.
    std::string header(const std::string &name) const
    {
        const auto it = headers.find(toLowerHelper(name));
        return (it == headers.end()) ? std::string{} : it->second;
    }
};

/// Response handed back to WebServer for serialization on the socket.
struct HttpResponse
{
    int status{200};
    std::string contentType{"application/json"};
    std::string body;

    /// HEADER-ONLY: do not define in .cpp.
    static HttpResponse json(const std::string &payload, int code = 200)
    {
        HttpResponse response;
        response.status = code;
        response.contentType = "application/json";
        response.body = payload;
        return response;
    }

    /// HEADER-ONLY: do not define in .cpp.
    static HttpResponse text(const std::string &payload, int code = 200)
    {
        HttpResponse response;
        response.status = code;
        response.contentType = "text/plain; charset=utf-8";
        response.body = payload;
        return response;
    }
};

/// Reason phrase for a status code; "Unknown" for codes we do not emit.
/// HEADER-ONLY: do not define in .cpp.
inline std::string statusText(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    default:
        return "Unknown";
    }
}

/// Convenience JSON error payload: {"error":"..."}.
/// HEADER-ONLY: do not define in .cpp.
inline std::string jsonError(const std::string &message)
{
    return "{\"error\":\"" + jsonEscape(message) + "\"}";
}

/// Parse "a=1&b=two" (also accepts a full "/path?a=1" target).
/// Percent-escapes are decoded; '+' becomes a space.
/// HEADER-ONLY: do not define in .cpp.
inline std::map<std::string, std::string> parseQuery(const std::string &target)
{
    std::map<std::string, std::string> result;

    const auto question = target.find('?');
    const std::string query = (question == std::string::npos) ? target : target.substr(question + 1);

    std::size_t pos = 0;
    while (pos <= query.size() && !query.empty())
    {
        const auto ampersand = query.find('&', pos);
        const std::string pair = query.substr(pos, (ampersand == std::string::npos) ? std::string::npos : ampersand - pos);

        if (!pair.empty())
        {
            const auto equals = pair.find('=');
            const std::string rawKey = (equals == std::string::npos) ? pair : pair.substr(0, equals);
            const std::string rawValue = (equals == std::string::npos) ? std::string{} : pair.substr(equals + 1);

            const auto decode = [](const std::string &input)
            {
                std::string decoded;
                decoded.reserve(input.size());
                for (std::size_t i = 0; i < input.size(); ++i)
                {
                    const char c = input[i];
                    if (c == '+')
                    {
                        decoded += ' ';
                    }
                    else if (c == '%' && i + 2 < input.size())
                    {
                        const auto hex = input.substr(i + 1, 2);
                        const auto value = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                        decoded += value;
                        i += 2;
                    }
                    else
                    {
                        decoded += c;
                    }
                }
                return decoded;
            };

            result[decode(rawKey)] = decode(rawValue);
        }

        if (ampersand == std::string::npos)
            break;
        pos = ampersand + 1;
    }

    return result;
}

/// Parse a raw request head + optional body. Returns false when the request
/// line or a header is malformed (caller should answer 400).
/// HEADER-ONLY: do not define in .cpp.
inline bool parseRequest(const std::string &raw, HttpRequest &out)
{
    out = HttpRequest{};

    const auto headerEnd = raw.find("\r\n\r\n");
    const std::size_t headerLength = (headerEnd == std::string::npos) ? raw.size() : headerEnd;
    const std::string head = raw.substr(0, headerLength);

    const auto requestLineEnd = head.find("\r\n");
    const std::string requestLine = head.substr(0, requestLineEnd);

    const auto firstSpace = requestLine.find(' ');
    const auto secondSpace = (firstSpace == std::string::npos)
                                 ? std::string::npos
                                 : requestLine.find(' ', firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos)
        return false;

    out.method = requestLine.substr(0, firstSpace);
    const std::string target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const std::string version = requestLine.substr(secondSpace + 1);

    if (out.method.empty() || target.empty() || version.rfind("HTTP/1.", 0) != 0)
        return false;
    if (target.front() != '/')
        return false;

    const auto question = target.find('?');
    out.path = (question == std::string::npos) ? target : target.substr(0, question);
    if (question != std::string::npos)
        out.query = parseQuery(target);

    std::size_t pos = (requestLineEnd == std::string::npos) ? std::string::npos : requestLineEnd + 2;
    while (pos != std::string::npos && pos < head.size())
    {
        const auto lineEnd = head.find("\r\n", pos);
        const std::string line = head.substr(pos, (lineEnd == std::string::npos) ? std::string::npos : lineEnd - pos);
        pos = (lineEnd == std::string::npos) ? std::string::npos : lineEnd + 2;

        if (line.empty())
            break;

        const auto colon = line.find(':');
        if (colon == std::string::npos)
            return false;

        const std::string name = toLowerHelper(trimHelper(line.substr(0, colon)));
        if (name.empty())
            return false;

        out.headers[name] = trimHelper(line.substr(colon + 1));
    }

    if (headerEnd != std::string::npos)
        out.body = raw.substr(headerEnd + 4);

    return true;
}

/// Render a response. Always closes the connection, so every reply carries an
/// accurate Content-Length and no chunking.
/// HEADER-ONLY: do not define in .cpp.
inline std::string serializeResponse(const HttpResponse &response)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << ' ' << statusText(response.status) << "\r\n"
        << "Content-Type: " << response.contentType << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << response.body;
    return out.str();
}
