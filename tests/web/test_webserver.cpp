#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

#include "common/Protocol.hpp"
#include "tests/TestSupport.hpp"
#include "web/HttpMessage.hpp"
#include "web/WebServer.hpp"

/// Tests the WebServer contract from web/WebServer.hpp:
///   - dispatch(): exact "METHOD /path" routing, 404 vs 405, request forwarding
///   - start()/stop(): lifecycle, failure to bind, idempotent stop
///   - run(): one request per connection over a real loopback socket,
///     8 KiB cap, partial reads, 400/404/405 replies, accept loop continuity
///
/// Socket clients use a receive timeout so a missing reply fails the test
/// instead of hanging it. Ports are probed, not hard-coded.

namespace
{

constexpr int kClientTimeoutMs = 2000;

HttpRequest makeRequest(const std::string &method, const std::string &path)
{
    HttpRequest request;
    request.method = method;
    request.path = path;
    return request;
}

std::string httpRequest(const std::string &method, const std::string &path)
{
    return method + " " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
}

int statusOf(const std::string &wire)
{
    if (wire.rfind("HTTP/1.1 ", 0) != 0 || wire.size() < 12)
        return -1;
    return std::atoi(wire.c_str() + 9);
}

std::string bodyOf(const std::string &wire)
{
    const std::size_t headerEnd = wire.find("\r\n\r\n");
    return headerEnd == std::string::npos ? std::string{} : wire.substr(headerEnd + 4);
}

/// Ask the kernel for a currently free TCP port and release it. The WebServer
/// then binds it with SO_REUSEADDR. A tiny race remains, acceptable for a
/// loopback unit test and better than a hard-coded port shared with the world.
std::uint16_t freePort()
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;

    const int enable = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof enable);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;

    std::uint16_t port = 0;
    if (::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof addr) == 0)
    {
        socklen_t length = sizeof addr;
        if (::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &length) == 0)
            port = ntohs(addr.sin_port);
    }

    ::close(fd);
    return port;
}

int connectTo(std::uint16_t port)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            return -1;

        timeval timeout{};
        timeout.tv_sec = kClientTimeoutMs / 1000;
        timeout.tv_usec = (kClientTimeoutMs % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (::connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof addr) == 0)
            return fd;

        const int error = errno;
        ::close(fd);
        if (error != ECONNREFUSED && error != EINTR)
            return -1;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return -1;
}

bool sendAll(int fd, const std::string &data)
{
    std::size_t sent = 0;
    while (sent < data.size())
    {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

/// Read until the peer closes (the contract is one request per connection and
/// Connection: close), or until the client receive timeout fires.
std::string readAll(int fd)
{
    std::string out;
    char buffer[4096];
    for (;;)
    {
        const ssize_t n = ::recv(fd, buffer, sizeof buffer, 0);
        if (n > 0)
        {
            out.append(buffer, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0)
            break;
        if (errno == EINTR)
            continue;
        break; // timeout or error
    }
    return out;
}

std::string request(std::uint16_t port, const std::string &raw)
{
    const int fd = connectTo(port);
    if (fd < 0)
        return {};

    if (!sendAll(fd, raw))
    {
        ::close(fd);
        return {};
    }

    const std::string response = readAll(fd);
    ::close(fd);
    return response;
}

} // namespace

int main()
{
    // --- dispatch() needs no socket ------------------------------------------
    {
        WebServer server;

        // No routes registered: everything is unknown.
        CHECK_EQ(server.dispatch(makeRequest("GET", proto::kApiHealth)).status, 404);

        std::atomic<int> calls{0};
        server.route("GET", proto::kApiHealth,
                     [&calls](const HttpRequest &)
                     {
                         ++calls;
                         return HttpResponse::json("{\"ok\":true}");
                     });
        server.route("POST", "/api/echo",
                     [](const HttpRequest &request)
                     {
                         return HttpResponse::text(request.body);
                     });

        const HttpResponse health = server.dispatch(makeRequest("GET", proto::kApiHealth));
        CHECK_EQ(health.status, 200);
        CHECK_EQ(health.body, std::string("{\"ok\":true}"));
        CHECK_EQ(health.contentType, std::string("application/json"));
        CHECK_EQ(calls.load(), 1);

        // Known path, wrong method -> 405.
        const HttpResponse wrongMethod = server.dispatch(makeRequest("POST", proto::kApiHealth));
        CHECK_EQ(wrongMethod.status, 405);
        CHECK(wrongMethod.body.find("\"error\"") != std::string::npos);
        CHECK_EQ(calls.load(), 1);

        // Exact path match: a trailing slash is a different route.
        CHECK_EQ(server.dispatch(makeRequest("GET", "/api/health/")).status, 404);

        // The parsed request reaches the handler intact.
        HttpRequest echo = makeRequest("POST", "/api/echo");
        echo.body = "payload";
        const HttpResponse echoed = server.dispatch(echo);
        CHECK_EQ(echoed.status, 200);
        CHECK_EQ(echoed.body, std::string("payload"));
        CHECK_EQ(echoed.contentType, std::string("text/plain; charset=utf-8"));
    }

    // --- stop() before start() is a no-op ------------------------------------
    {
        WebServer idle;
        CHECK(!idle.isRunning());
        idle.stop();
        idle.stop();
        CHECK(!idle.isRunning());
    }

    // --- lifecycle and bind failure ------------------------------------------
    {
        const std::uint16_t port = freePort();
        CHECK(port != 0);

        WebServer first;
        CHECK(first.start(port));
        CHECK(first.isRunning());

        // A second server on the same port must fail cleanly and leave the
        // first one running.
        WebServer second;
        CHECK(!second.start(port));
        CHECK(!second.isRunning());
        second.stop();
        CHECK(first.isRunning());

        first.stop();
        CHECK(!first.isRunning());
        first.stop(); // idempotent
        CHECK(!first.isRunning());
    }

    // --- run(): real requests over loopback ----------------------------------
    {
        const std::uint16_t port = freePort();
        CHECK(port != 0);

        WebServer server;
        std::atomic<int> healthHits{0};
        server.route("GET", proto::kApiHealth,
                     [&healthHits](const HttpRequest &)
                     {
                         ++healthHits;
                         return HttpResponse::json("{\"ok\":true}");
                     });

        CHECK(server.start(port));
        CHECK(server.isRunning());

        // 200 with a complete, correctly framed response.
        const std::string ok = request(port, httpRequest("GET", proto::kApiHealth));
        CHECK_EQ(statusOf(ok), 200);
        CHECK_EQ(bodyOf(ok), std::string("{\"ok\":true}"));
        CHECK(ok.find("Content-Type: application/json\r\n") != std::string::npos);
        CHECK(ok.find("Content-Length: 11\r\n") != std::string::npos);
        CHECK(ok.find("Connection: close\r\n") != std::string::npos);
        CHECK_EQ(healthHits.load(), 1);

        // Unknown path -> 404; known path with the wrong method -> 405.
        const std::string missing = request(port, httpRequest("GET", "/api/missing"));
        CHECK_EQ(statusOf(missing), 404);
        CHECK(bodyOf(missing).find("\"error\"") != std::string::npos);

        const std::string wrongMethod = request(port, httpRequest("POST", proto::kApiHealth));
        CHECK_EQ(statusOf(wrongMethod), 405);

        // Unparseable request line -> 400, and the server keeps serving.
        const std::string garbage = request(port, "GARBAGE\r\n\r\n");
        CHECK_EQ(statusOf(garbage), 400);
        CHECK(bodyOf(garbage).find("\"error\"") != std::string::npos);

        const std::string again = request(port, httpRequest("GET", proto::kApiHealth));
        CHECK_EQ(statusOf(again), 200);
        CHECK_EQ(healthHits.load(), 2);

        // TCP is a byte stream: one request may arrive in several recv()s.
        {
            const int fd = connectTo(port);
            CHECK(fd >= 0);
            if (fd >= 0)
            {
                CHECK(sendAll(fd, "GET /api/hea"));
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                CHECK(sendAll(fd, "lth HTTP/1.1\r\nHost: localhost\r\n\r\n"));

                const std::string split = readAll(fd);
                ::close(fd);
                CHECK_EQ(statusOf(split), 200);
                CHECK_EQ(healthHits.load(), 3);
            }
        }

        // More than 8 KiB without a header terminator must answer 400 rather
        // than wait forever or grow the buffer without bound.
        {
            std::string oversized = "GET /api/health HTTP/1.1\r\n";
            oversized.append(9000, 'a');
            const std::string tooLarge = request(port, oversized);
            CHECK_EQ(statusOf(tooLarge), 400);
        }

        server.stop();
        CHECK(!server.isRunning());
        CHECK_EQ(healthHits.load(), 3);
    }

    TEST_RETURN();
}
