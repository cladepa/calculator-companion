#include "http_client.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>

#define CONNECT_TIMEOUT_SEC 8
#define IO_TIMEOUT_SEC 10
#define RESPONSE_BUF_SIZE 4096

static void set_err(char *err_buf, size_t err_buf_size, const char *msg) {
    if (err_buf != NULL && err_buf_size > 0) {
        snprintf(err_buf, err_buf_size, "%s", msg);
    }
}

/* Parses "http://host[:port][/path]" into its components. Returns 0 on
   success, -1 if the scheme is missing/unsupported - https:// lands here
   too, deliberately, until TLS support is added as its own step. */
static int parse_http_url(const char *url, char *host, size_t hostsz,
                           char *port, size_t portsz,
                           char *path, size_t pathsz) {
    const char *prefix = "http://";
    size_t prefix_len = strlen(prefix);
    if (strncmp(url, prefix, prefix_len) != 0) {
        return -1;
    }
    const char *rest = url + prefix_len;

    const char *slash = strchr(rest, '/');
    size_t hostport_len = slash != NULL ? (size_t)(slash - rest) : strlen(rest);

    char hostport[320];
    if (hostport_len == 0 || hostport_len >= sizeof(hostport)) return -1;
    memcpy(hostport, rest, hostport_len);
    hostport[hostport_len] = '\0';

    char *colon = strchr(hostport, ':');
    if (colon != NULL) {
        *colon = '\0';
        snprintf(host, hostsz, "%.*s", (int)(hostsz - 1), hostport);
        snprintf(port, portsz, "%.*s", (int)(portsz - 1), colon + 1);
    } else {
        snprintf(host, hostsz, "%.*s", (int)(hostsz - 1), hostport);
        snprintf(port, portsz, "%s", "80");
    }

    if (slash != NULL) {
        snprintf(path, pathsz, "%.*s", (int)(pathsz - 1), slash);
    } else {
        snprintf(path, pathsz, "%s", "/");
    }
    return 0;
}

/* Resolves host:port and connects with a bounded timeout, trying every
   address getaddrinfo returns until one works. Returns a connected,
   blocking socket fd, or -1 on failure/timeout. */
static int connect_with_timeout(const char *host, const char *port,
                                 char *err_buf, size_t err_buf_size) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        set_err(err_buf, err_buf_size, "DNS resolution failed");
        return -1;
    }

    int sock = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) continue;

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(sock, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            fcntl(sock, F_SETFL, flags);
            break;
        }
        if (errno == EINPROGRESS) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);
            struct timeval tv = { .tv_sec = CONNECT_TIMEOUT_SEC, .tv_usec = 0 };
            int sel = select(sock + 1, NULL, &wfds, NULL, &tv);
            if (sel > 0) {
                int soerr = 0;
                socklen_t len = sizeof(soerr);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &len);
                if (soerr == 0) {
                    fcntl(sock, F_SETFL, flags);
                    break;
                }
            }
        }
        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock == -1) {
        set_err(err_buf, err_buf_size, "connect() failed or timed out");
        return -1;
    }

    struct timeval io_tv = { .tv_sec = IO_TIMEOUT_SEC, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &io_tv, sizeof(io_tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &io_tv, sizeof(io_tv));

    return sock;
}

int http_post(const char *url, const char *body, int *status_out,
              char *err_buf, size_t err_buf_size) {
    char host[256], port[16], path[512];
    if (parse_http_url(url, host, sizeof(host), port, sizeof(port), path, sizeof(path)) != 0) {
        set_err(err_buf, err_buf_size, "unsupported URL (only http:// is implemented so far)");
        return -1;
    }

    int sock = connect_with_timeout(host, port, err_buf, err_buf_size);
    if (sock < 0) return -1;

    size_t body_len = body != NULL ? strlen(body) : 0;
    char request[8192];
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, body_len, body != NULL ? body : "");

    if (req_len < 0 || (size_t)req_len >= sizeof(request)) {
        close(sock);
        set_err(err_buf, err_buf_size, "request too large for internal buffer");
        return -1;
    }

    ssize_t sent_total = 0;
    while (sent_total < req_len) {
        ssize_t n = send(sock, request + sent_total, (size_t)(req_len - sent_total), 0);
        if (n <= 0) {
            close(sock);
            set_err(err_buf, err_buf_size, "send() failed or timed out");
            return -1;
        }
        sent_total += n;
    }

    char response[RESPONSE_BUF_SIZE];
    size_t resp_len = 0;
    ssize_t n;
    while (resp_len < sizeof(response) - 1 &&
           (n = recv(sock, response + resp_len, sizeof(response) - 1 - resp_len, 0)) > 0) {
        resp_len += (size_t)n;
    }
    response[resp_len] = '\0';
    close(sock);

    if (resp_len == 0) {
        set_err(err_buf, err_buf_size, "no response received");
        return -1;
    }

    int status = 0;
    if (sscanf(response, "HTTP/%*d.%*d %d", &status) != 1) {
        set_err(err_buf, err_buf_size, "could not parse HTTP status line");
        return -1;
    }

    if (status_out != NULL) *status_out = status;

    if (status < 200 || status >= 300) {
        set_err(err_buf, err_buf_size, "non-2xx HTTP status");
        return -1;
    }

    return 0;
}
