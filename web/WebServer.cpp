#include "WebServer.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <unistd.h>

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
}
