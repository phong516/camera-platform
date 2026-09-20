#pragma once

#include <cstdint>

#include "model/CameraState.hpp"
#include "model/SystemStatus.hpp"
#include "web/WebServer.hpp"

/// Everything the HTTP API is allowed to do to the application.
///
/// Implemented by Application. This interface exists so the web layer depends
/// on an abstraction instead of on the app executable (no circular link) and
/// so tests can substitute a fake controller.
class CameraControl
{
public:
    virtual ~CameraControl() = default;

    virtual CameraState cameraState() const = 0;
    virtual SystemStatus systemStatus() const = 0;

    virtual bool startCamera() = 0;
    virtual bool stopCamera() = 0;
    virtual bool setResolution(std::uint32_t width, std::uint32_t height) = 0;
    virtual bool setFrameRate(std::uint32_t fpsNum, std::uint32_t fpsDenom) = 0;
};

/// Maps HTTP routes onto CameraControl. Holds no state of its own.
///
/// STATUS: header only. ApiController.cpp is the implementation task.
///
/// Routes (README lines 306-310):
///   GET  /api/status        -> 200, toJson(cameraState())+toJson(systemStatus())
///   POST /api/camera/start  -> 200 when startCamera() succeeds, 500 otherwise
///   POST /api/camera/stop   -> 200 when stopCamera() succeeds, 500 otherwise
///   GET  /api/config        -> 200, current resolution / frame rate / source
///   POST /api/config        -> 200 when setResolution()/setFrameRate() succeed,
///                              400 when width/height/fps are missing or invalid
class ApiController
{
public:
    explicit ApiController(CameraControl &control);

    void registerRoutes(WebServer &server);

    HttpResponse getStatus(const HttpRequest &request);
    HttpResponse startCamera(const HttpRequest &request);
    HttpResponse stopCamera(const HttpRequest &request);
    HttpResponse getConfig(const HttpRequest &request);
    HttpResponse updateConfig(const HttpRequest &request);

private:
    CameraControl &m_control;
};
