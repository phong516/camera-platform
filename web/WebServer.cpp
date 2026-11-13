#include "WebServer.hpp"
#include "HttpMessage.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cerrno>
#include <cstddef>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

WebServer::~WebServer()
{
    stop();
}

bool WebServer::start(std::uint16_t port)
{
    if (m_running)
    {
        std::cerr << "WebServer is running. Cannot start\n";
        return false;
    }
    m_port = port;
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0)
    {
        std::cerr << "Failed to create socket\n";
        return false;
    }

    int enable = 1;
    if (setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &enable, static_cast<socklen_t>(sizeof(enable))) == -1)
    {
        std::cerr << "Failed to setsockopt\n";
        close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET, .sin_port = htons(m_port), .sin_addr = {.s_addr = htonl(INADDR_ANY)}};
    socklen_t addrLen = sizeof(addr);

    if (bind(m_listenFd, reinterpret_cast<const struct sockaddr *>(&addr), addrLen) == -1)
    {
        if (errno == EADDRINUSE)
        {
            std::cerr << "Port is already in use\n";
        }
        std::cerr << "Failed to bind\n";
        return false;
    }

    if (listen(m_listenFd, 1) == -1)
    {
        std::cerr << "Failed to listen\n";
        return false;
    }

    m_running = true;
    m_thread = std::thread(&WebServer::run, this);

    return true;
}
void WebServer::stop()
{
    m_running = false;
    if (m_listenFd >= 0)
    {
        shutdown(m_listenFd, SHUT_RDWR);
    }
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    if (m_listenFd >= 0)
    {
        close(m_listenFd);
        m_listenFd = -1;
    }
}
bool WebServer::isRunning() const
{
    return m_running;
}
void WebServer::route(const std::string &method, const std::string &path, HttpHandler handler)
{
    m_routes[path][method] = handler;
}
HttpResponse WebServer::dispatch(const HttpRequest &request) const
{
    auto pathIt = m_routes.find(request.path);
    // no path -> error 404
    if (pathIt == m_routes.end())
    {
        return HttpResponse::json(jsonError("invalid path"), 404);
    }
    auto methodIt = pathIt->second.find(request.method);
    // no method -> error 405
    if (methodIt == pathIt->second.end())
    {
        return HttpResponse::json(jsonError("invalid method"), 405);
    }
    return methodIt->second(request);
}
void WebServer::run()
{
    while (isRunning())
    {
        int fd = accept(m_listenFd, NULL, NULL);
        if (fd < 0)
        {
            continue;
        }
        struct timeval tv = {.tv_sec = CONNECTION_TIMEOUT_SECOND};
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, static_cast<socklen_t>(sizeof(tv))) == -1)
        {
            std::cerr << "Failed to setsockopt\n";
            close(fd);
            continue;
        }

        std::string raw{""};
        char buf[BUFFER_SIZE]{};
        while (raw.size() < BUFFER_SIZE)
        {
            int n = recv(fd, buf, BUFFER_SIZE - raw.size(), 0);
            if (n > 0)
            {
                raw.append(buf, static_cast<std::size_t>(n));
                if (raw.contains("\r\n\r\n"))
                {
                    break;
                }
            }
            if (n == 0)
            {
                break;
            }
            if (n < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    continue;
                }
                break;
            }
        }

        HttpRequest req;
        HttpResponse resp;
        if (parseRequest(raw, req))
        {
            resp = dispatch(req);
        }
        else
        {
            resp = HttpResponse::json(jsonError("bad request"), 400);
        }

        std::string out{serializeResponse(resp)};

        // send all
        std::size_t sent{0};
        while (sent < out.size())
        {
            ssize_t s = send(fd, out.c_str(), out.size() - sent, 0);
            if (s < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    continue;
                }
                break;
            }
            if (s == 0)
            {
                break;
            }
            sent += static_cast<size_t>(s);
        }
        close(fd);
    }
}
