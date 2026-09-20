#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

#include "web/HttpMessage.hpp"

using HttpHandler = std::function<HttpResponse(const HttpRequest &)>;

/// Minimal single-threaded HTTP/1.1 server built directly on POSIX sockets.
///
/// STATUS: header only. WebServer.cpp is the implementation task.
///
/// Contract for WebServer.cpp:
///   - start(): socket() -> SO_REUSEADDR -> bind() -> listen(16) -> spawn run()
///     in m_thread; return false on any failure and leave m_listenFd == -1
///   - run(): accept() loop; per connection read up to 8 KiB with a 5 s
///     SO_RCVTIMEO, parseRequest(), dispatch(), write serializeResponse()
///   - one request per connection, always reply (400 on unparseable input)
///   - an unhandled path answers 404, a known path with the wrong method 405
///   - stop(): set m_running = false, shutdown() the listen fd to break accept(),
///     close() it, then join the thread (guard with m_thread.joinable())
///   - ~WebServer() calls stop(); stop() on a never-started server is a no-op
class WebServer
{
public:
    WebServer() = default;
    ~WebServer();

    bool start(std::uint16_t port = 8080);
    void stop();
    bool isRunning() const;

    /// Register a handler. The key is "METHOD /path", e.g. "GET /api/status".
    void route(const std::string &method, const std::string &path, HttpHandler handler);

    /// Look up the handler for a parsed request; synthesizes 404/405 replies.
    HttpResponse dispatch(const HttpRequest &request) const;

private:
    int m_listenFd{-1};
    std::uint16_t m_port{0};
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::unordered_map<std::string, HttpHandler> m_routes;

    void run();

    WebServer(const WebServer &) = delete;
    WebServer &operator=(const WebServer &) = delete;
    WebServer(WebServer &&) = delete;
    WebServer &operator=(WebServer &&) = delete;
};
