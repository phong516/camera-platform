#include "ApiController.hpp"
#include "HttpMessage.hpp"
#include "Profile.hpp"
#include "Protocol.hpp"
#include "SystemStatus.hpp"
#include <string_view>

namespace
{

std::vector<std::string_view> extractPathSegment(std::string_view path, const std::string &prefix,
                                                 const char delimiter = '/')
{
    size_t prefixPos = path.find(prefix);
    if (prefixPos == std::string::npos)
    {
        return {};
    }
    std::vector<std::string_view> segments{};

    path.remove_prefix(prefixPos + prefix.size());

    if (path.front() == delimiter)
    {
        path.remove_prefix(1);
    }
    while (!path.empty())
    {
        auto pos = path.find(delimiter);
        segments.push_back(path.substr(0, pos));
        if (pos == std::string_view::npos)
        {
            break;
        }
        path.remove_prefix(pos + 1);
    }
    return segments;
}

} // namespace

ApiController::ApiController(CameraControl &control) : m_control(control)
{
}

void ApiController::registerRoutes(WebServer &server)
{
    if (server.isRunning())
    {
        return;
    }
    //	GET	/api/health
    server.route("GET", proto::kApiHealth, [this](const HttpRequest &request) { return getHealth(request); });
    //	GET	/api/status
    server.route("GET", proto::kApiStatus, [this](const HttpRequest &request) { return getStatus(request); });
    //	GET	/api/profiles
    server.route("GET", proto::kApiProfiles, [this](const HttpRequest &request) { return getProfiles(request); });

    for (const auto &profile : m_control.profiles())
    {
        //	GET	/api/profiles/{name}
        server.route("GET", std::string(proto::kApiProfiles) + '/' + profile.name.c_str(),
                     [this](const HttpRequest &request) { return getProfile(request); });
        //	POST	/api/profiles/{name}/on
        server.route("POST", proto::profileActionPath(profile.name, proto::kActionOn),
                     [this](const HttpRequest &request) { return onProfile(request); });
        //	POST	/api/profiles/{name}/off
        server.route("POST", proto::profileActionPath(profile.name, proto::kActionOff),
                     [this](const HttpRequest &request) { return offProfile(request); });
    }
}
HttpResponse ApiController::getHealth(const HttpRequest &request)
{
    return HttpResponse::json("{\"ok\":true}", 200);
}
HttpResponse ApiController::getStatus(const HttpRequest &request)
{
    return HttpResponse::json(toJson(m_control.systemStatus()), 200);
}
HttpResponse ApiController::getProfiles(const HttpRequest &request)
{
    HttpResponse resp{HttpResponse::json("{\"profiles\":[", 200)};
    for (const auto &profile : m_control.profiles())
    {
        resp.body.append(toJson(profile) + ',');
    }
    resp.body.pop_back();
    resp.body.append("]}");
    return resp;
}
HttpResponse ApiController::getProfile(const HttpRequest &request)
{
    std::vector<std::string_view> segments = extractPathSegment(request.path, proto::kApiProfiles);

    if (segments.size() != 1)
    {
        return HttpResponse::json(jsonError("invalid path"), 404);
    }

    const std::string_view &profileName = segments[0];

    const auto &profiles = m_control.profiles();
    auto it = std::find_if(profiles.begin(), profiles.end(),
                           [&profileName](const auto &profile) { return profile.name == profileName; });
    if (it == profiles.end())
    {
        return HttpResponse::json(jsonError("invalid profile name"), 404);
    }

    return HttpResponse::json(toJson(*it));
}
HttpResponse ApiController::onProfile(const HttpRequest &request)
{
    std::vector<std::string_view> segments = extractPathSegment(request.path, proto::kApiProfiles);
    if (segments.empty())
    {
        return HttpResponse::json(jsonError("invalid path"), 404);
    }

    const std::string_view &profileName = segments[0];

    if (profileResponse(profileName).status != 200)
    {
        return HttpResponse::json(jsonError("invalid profile name"), 404);
    }

    m_control.setProfileEnabled(std::string(profileName), true);

    return profileResponse(profileName);
}
HttpResponse ApiController::offProfile(const HttpRequest &request)
{
    std::vector<std::string_view> segments = extractPathSegment(request.path, proto::kApiProfiles);
    if (segments.empty())
    {
        return HttpResponse::json(jsonError("invalid path"), 404);
    }

    const std::string_view &profileName = segments[0];

    if (profileResponse(profileName).status != 200)
    {
        return HttpResponse::json(jsonError("invalid profile name"), 404);
    }

    m_control.setProfileEnabled(std::string(profileName), false);

    return profileResponse(profileName);
}
HttpResponse ApiController::profileResponse(std::string_view name) const
{
    const auto &profiles = m_control.profiles();
    auto it =
        std::find_if(profiles.begin(), profiles.end(), [&name](const auto &profile) { return profile.name == name; });
    if (it == profiles.end())
    {
        return HttpResponse::json(jsonError("invalid profile name"), 404);
    }
    return HttpResponse::json(toJson(*it));
}
