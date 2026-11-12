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
}

bool WebServer::start(std::uint16_t port)
{
    m_port = port;
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0)
    {
        std::cerr << "Failed to create socket\n";
        return false;
    }

    int enable = 1;
    if (setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &enable, (socklen_t)sizeof(enable)) == -1)
    {
        std::cerr << "Failed to setsockopt\n";
        return false;
    }

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = m_port, .sin_addr = {.s_addr = htonl(INADDR_ANY)}};
    socklen_t addrLen = sizeof(addr);

    if (bind(m_listenFd, (const struct sockaddr *)&addr, addrLen) == -1)
    {
        std::cerr << "Failed to bind\n";
        return false;
    }

    if (listen(m_listenFd, 1) == -1)
    {
        std::cerr << "Failed to listen\n";
        return false;
    }

    m_thread = std::thread(&WebServer::run, this);

    return true;
}
void WebServer::stop()
{
}
bool WebServer::isRunning() const
{
    return {};
}
void WebServer::route(const std::string &method, const std::string &path, HttpHandler handler)
{
}
HttpResponse WebServer::dispatch(const HttpRequest &request) const
{
    return {};
}
void WebServer::run()
{
    while (!m_running)
    {
        int fd = accept(m_listenFd, NULL, NULL);
        if (fd < 0)
        {
            continue;
        }
        struct timeval tv = {.tv_sec = CONNECTION_TIMEOUT_SECOND};
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv) == -1))
        {
            std::cerr << "Failed to setsockopt\n";
            continue;
        }

        std::string raw{""};
        char buf[BUFFER_SIZE]{};
        while (raw.size() < BUFFER_SIZE)
        {
            int n = recv(fd, buf, BUFFER_SIZE - raw.size(), 0);
            if (n > 0)
            {
                raw += buf;
                if (raw.ends_with("\r\n\r\n"))
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
                if (errno == EINTR)
                {
                    continue;
                }
                break;
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
                ssize_t size = send(fd, out.c_str(), out.size() - sent, 0);
            }
            ssize_t s = send(fd, out.c_str(), sizeof(out), 0);
            if (s < 0)
            {
                if (errno == EINTR)
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
