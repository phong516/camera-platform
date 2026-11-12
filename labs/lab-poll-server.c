/* lab-poll-server.c — one thread, many HTTP clients, poll(2)
 *
 * Serves GET /api/status with a JSON snapshot that counts requests. One
 * request per connection; the connection closes after the response.
 *
 * The point is not the HTTP: it is the event loop. The server blocks in
 * poll(), wakes when any socket is ready, and services only those sockets.
 * A client that connects and sends nothing costs one table slot, not a
 * thread and not the whole server.
 *
 * Build:
 *   gcc -std=c11 -Wall -Wextra -Wpedantic -g -O0 \
 *       labs/lab-poll-server.c -o ~/cam-recon/pollserver
 *
 * Run:
 *   ~/cam-recon/pollserver          # port 18080
 *   ~/cam-recon/pollserver 9000
 *
 * Probe:
 *   curl -s http://127.0.0.1:18080/api/status
 *   printf 'GET /api/status HTTP/1.1\r\nHost: x\r\n\r\n' | nc 127.0.0.1 18080
 */
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 64
#define REQUEST_MAX 8192
#define DEFAULT_PORT 18080

struct client
{
    int fd;
    size_t used;
    char buf[REQUEST_MAX + 1]; /* +1 so a complete buffer is always NUL-terminated */
};

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int set_nonblocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int send_all(int fd, const char *data, size_t len)
{
    size_t off = 0;
    while (off < len)
    {
        const ssize_t n = send(fd, data + off, len - off, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        off += (size_t)n;
    }
    return 0;
}

static void respond(int fd, int status, const char *reason, const char *type, const char *body)
{
    char head[512];
    const int n = snprintf(head, sizeof head,
                           "HTTP/1.1 %d %s\r\n"
                           "Content-Type: %s\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           status, reason, type, strlen(body));
    if (n < 0 || (size_t)n >= sizeof head)
        return;
    if (send_all(fd, head, (size_t)n) == 0)
        send_all(fd, body, strlen(body));
}

static int has_header_end(const char *buf, size_t len)
{
    for (size_t i = 0; i + 3 < len; ++i)
        if (memcmp(buf + i, "\r\n\r\n", 4) == 0)
            return 1;
    return 0;
}

static void handle_request(struct client *c, unsigned long *requests, long started_ms, int active)
{
    char method[16];
    char target[256];
    char version[16];

    if (sscanf(c->buf, "%15s %255s %15s", method, target, version) != 3)
    {
        respond(c->fd, 400, "Bad Request", "application/json", "{\"error\":\"bad request line\"}");
        return;
    }

    if (strcmp(target, "/api/status") != 0)
    {
        respond(c->fd, 404, "Not Found", "application/json", "{\"error\":\"not found\"}");
        return;
    }

    if (strcmp(method, "GET") != 0)
    {
        respond(c->fd, 405, "Method Not Allowed", "application/json", "{\"error\":\"use GET\"}");
        return;
    }

    ++*requests;
    const long uptime = (now_ms() - started_ms) / 1000;
    char body[256];
    snprintf(body, sizeof body,
             "{\"status\":\"ok\",\"requests\":%lu,\"uptimeSeconds\":%ld,\"activeClients\":%d}",
             *requests, uptime, active);
    respond(c->fd, 200, "OK", "application/json", body);
    printf("[req] %s %s -> 200 (requests=%lu, active=%d)\n", method, target, *requests, active);
}

static void close_client(struct client *c)
{
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    c->used = 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int port = DEFAULT_PORT;
    if (argc > 1)
    {
        char *end = NULL;
        const long parsed = strtol(argv[1], &end, 10);
        if (*argv[1] == '\0' || *end != '\0' || parsed < 0 || parsed > 65535)
        {
            fprintf(stderr, "usage: %s [port]\n", argv[0]);
            return 2;
        }
        port = (int)parsed;
    }

    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        return 1;
    }

    const int enable = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof enable) < 0)
    {
        perror("setsockopt");
        close(listen_fd);
        return 1;
    }

    if (set_nonblocking(listen_fd) < 0)
    {
        perror("fcntl");
        close(listen_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0)
    {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) < 0)
    {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    socklen_t addr_len = sizeof addr;
    if (getsockname(listen_fd, (struct sockaddr *)&addr, &addr_len) == 0)
        port = ntohs(addr.sin_port);
    printf("[serve] listening on http://127.0.0.1:%d  (Ctrl-C to stop)\n", port);

    struct client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        clients[i].fd = -1;
        clients[i].used = 0;
    }

    const long started_ms = now_ms();
    unsigned long requests = 0;

    while (!g_stop)
    {
        struct pollfd pfds[MAX_CLIENTS + 1];
        int slot_for[MAX_CLIENTS + 1];

        pfds[0].fd = listen_fd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        slot_for[0] = -1;
        nfds_t nfds = 1;

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i].fd < 0)
                continue;
            pfds[nfds].fd = clients[i].fd;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            slot_for[nfds] = i;
            ++nfds;
        }

        const int ready = poll(pfds, nfds, 1000);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }
        if (ready == 0)
            continue; /* timeout: a place for housekeeping */

        if (pfds[0].revents & POLLIN)
        {
            for (;;)
            {
                struct sockaddr_in peer;
                socklen_t peer_len = sizeof peer;
                const int fd = accept(listen_fd, (struct sockaddr *)&peer, &peer_len);
                if (fd < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    if (errno == EINTR)
                        continue;
                    perror("accept");
                    break;
                }

                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; ++i)
                {
                    if (clients[i].fd < 0)
                    {
                        slot = i;
                        break;
                    }
                }

                char peer_text[INET_ADDRSTRLEN] = "?";
                inet_ntop(AF_INET, &peer.sin_addr, peer_text, sizeof peer_text);

                if (slot < 0)
                {
                    respond(fd, 503, "Service Unavailable", "application/json", "{\"error\":\"too many clients\"}");
                    close(fd);
                    printf("[busy] %s:%u rejected\n", peer_text, ntohs(peer.sin_port));
                    continue;
                }

                clients[slot].fd = fd;
                clients[slot].used = 0;
                printf("[open] %s:%u slot=%d\n", peer_text, ntohs(peer.sin_port), slot);
            }
        }

        for (nfds_t i = 1; i < nfds; ++i)
        {
            if (pfds[i].revents == 0)
                continue;
            struct client *c = &clients[slot_for[i]];
            if (c->fd < 0)
                continue;

            if (pfds[i].revents & (POLLERR | POLLNVAL))
            {
                close_client(c);
                continue;
            }

            if (pfds[i].revents & (POLLIN | POLLHUP))
            {
                const ssize_t n = recv(c->fd, c->buf + c->used, REQUEST_MAX - c->used, 0);
                if (n == 0)
                {
                    close_client(c);
                    continue;
                }
                if (n < 0)
                {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;
                    close_client(c);
                    continue;
                }

                c->used += (size_t)n;
                c->buf[c->used] = '\0';
                if (has_header_end(c->buf, c->used))
                {
                    int active = 0;
                    for (int j = 0; j < MAX_CLIENTS; ++j)
                        if (clients[j].fd >= 0)
                            ++active;
                    handle_request(c, &requests, started_ms, active);
                    close_client(c);
                }
                else if (c->used == REQUEST_MAX)
                {
                    respond(c->fd, 400, "Bad Request", "application/json", "{\"error\":\"request too large\"}");
                    close_client(c);
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; ++i)
        close_client(&clients[i]);
    close(listen_fd);
    printf("[serve] stopped: %lu requests\n", requests);
    return 0;
}
