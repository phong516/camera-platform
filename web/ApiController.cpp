#include "ApiController.hpp"
#include "HttpMessage.hpp"
#include "Profile.hpp"
#include "Protocol.hpp"
#include "SystemStatus.hpp"

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
        server.route("POST", proto::profileActionPath(profile.name, "on"),
                     [this](const HttpRequest &request) { return onProfile(request); });
        //	POST	/api/profiles/{name}/off
        server.route("POST", proto::profileActionPath(profile.name, "off"),
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
    //	get name of profile from path
    //	/profiles/{name}
    std::string path = request.path;
    while (path.back() == '/')
    {
        path.pop_back();
    }
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
    {
        return HttpResponse::json(jsonError("invalid path"), 404);
    }
    std::string name = path.substr(pos + 1);

    const auto &profiles = m_control.profiles();
    auto it =
        std::find_if(profiles.begin(), profiles.end(), [&name](const auto &profile) { return profile.name == name; });
    if (it == profiles.end())
    {
        return HttpResponse::json(jsonError("invalid profile name"), 404);
    }
    return HttpResponse::json(toJson(*it));
}
HttpResponse ApiController::onProfile(const HttpRequest &request)
{
    return {};
}
HttpResponse ApiController::offProfile(const HttpRequest &request)
{
    return {};
}
std::string ApiController::profileNameFromPath(const std::string &path)
{
	const std::string prefix = std::string(proto::kApiProfiles) + '/';
	auto prefixPos = path.find(prefix);
	if (prefixPos == std::string::npos)
	{
		return {};
	}
	auto profileNameIt = std::find(path.begin() + prefixPos + prefix.size(), path.end(), '/');
	if (profileNameIt == path.end())
	{
		return {};
	}
	auto profileNameEndIt = std::find(profileNameIt + 1, path.end(), '/');
	if (profileNameEndIt == path.end())
	{
		return {};
	}
	std::string profileName(profileNameIt, profileNameEndIt - 1);
    return profileName;
}
