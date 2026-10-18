#pragma once

#include <string>

#include "model/Profile.hpp"
#include "model/SystemStatus.hpp"
#include "web/CameraControl.hpp"
#include "web/WebServer.hpp"

/// Maps the control plane's HTTP routes (common/Protocol.hpp) onto
/// CameraControl. Holds no state of its own.
///
/// STATUS: header only. web/ApiController.cpp is the Phase 3 implementation
/// task and must be written against this contract.
///
/// Routes (docs/ARCHITECTURE-SERVER-CLIENT.md section 6):
///   GET  /api/health              -> getHealth
///   GET  /api/status              -> getStatus
///   GET  /api/profiles            -> getProfiles
///   GET  /api/profiles/{name}     -> getProfile
///   POST /api/profiles/{name}/on  -> onProfile
///   POST /api/profiles/{name}/off -> offProfile
///
/// Contract for ApiController.cpp:
///   - getHealth: 200 with {"ok":true}.
///   - getStatus: 200 with toJson(systemStatus()) - the SystemStatus JSON
///     ONLY, never concatenated with anything else.
///   - getProfiles: 200 with {"profiles":[...]} in catalog order.
///   - getProfile/onProfile/offProfile take the name from the request path
///     (path parsing is internal); each answers 200 with toJson(profile) -
///     for the actions, the state AFTER the change - or 404 jsonError when
///     the name is not in the catalog.
///   - on/off are idempotent (Protocol.hpp): setProfileEnabled returning true
///     for an already-changed profile is still 200.
///   - Route registration happens inside registerRoutes(); WebServer routes on
///     exact "METHOD /path" keys, so registerRoutes() enumerates
///     m_control.profiles() once and registers a concrete key per catalog
///     name. Unknown names then answer 404 the same way any unregistered path
///     does. Call registerRoutes() before WebServer::start().
class ApiController
{
public:
    explicit ApiController(CameraControl &control);

    /// Registers every route above on the server. Call once, before start().
    void registerRoutes(WebServer &server);

    // The per-route handlers share the WebServer::HttpHandler shape
    // (HttpResponse(const HttpRequest&)), so registerRoutes() binds them
    // directly. They stay public so a test can call them with a hand-built
    // HttpRequest: the test surface is the interface.

    HttpResponse getHealth(const HttpRequest &request);
    HttpResponse getStatus(const HttpRequest &request);
    HttpResponse getProfiles(const HttpRequest &request);
    HttpResponse getProfile(const HttpRequest &request);  ///< GET /api/profiles/{name}
    HttpResponse onProfile(const HttpRequest &request);   ///< POST /api/profiles/{name}/on
    HttpResponse offProfile(const HttpRequest &request);  ///< POST /api/profiles/{name}/off

private:
    CameraControl &m_control;
};
