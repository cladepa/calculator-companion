#include "https_client.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

#define IO_TIMEOUT_MS 10000

static void set_err(char *err_buf, size_t err_buf_size, const char *msg) {
    if (err_buf != NULL && err_buf_size > 0) {
        snprintf(err_buf, err_buf_size, "%s", msg);
    }
}

static void set_err_code(char *err_buf, size_t err_buf_size, const char *prefix, int code) {
    if (err_buf == NULL || err_buf_size == 0) return;
    char mbed_err[128];
    mbedtls_strerror(code, mbed_err, sizeof(mbed_err));
    snprintf(err_buf, err_buf_size, "%s: %s", prefix, mbed_err);
}

/* Same shape as http_client's URL parser, but default port 443. */
static int parse_https_url(const char *url, char *host, size_t hostsz,
                            char *port, size_t portsz,
                            char *path, size_t pathsz) {
    const char *prefix = "https://";
    size_t prefix_len = strlen(prefix);
    if (strncmp(url, prefix, prefix_len) != 0) return -1;
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
        snprintf(port, portsz, "%s", "443");
    }

    if (slash != NULL) {
        snprintf(path, pathsz, "%.*s", (int)(pathsz - 1), slash);
    } else {
        snprintf(path, pathsz, "%s", "/");
    }
    return 0;
}

int https_post(const char *url, const char *body, const char *ca_cert_path,
                int *status_out, char *err_buf, size_t err_buf_size) {
    char host[256], port[16], path[512];
    if (parse_https_url(url, host, sizeof(host), port, sizeof(port), path, sizeof(path)) != 0) {
        set_err(err_buf, err_buf_size, "not a valid https:// URL");
        return -1;
    }

    int ret;
    int rc = -1;

    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca_cert;

    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&ca_cert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    const char *pers = "guardd_https_client";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                 (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        set_err_code(err_buf, err_buf_size, "ctr_drbg_seed failed", ret);
        goto cleanup;
    }

    /* Deliberately no "skip verification" fallback: this channel can carry
       a victim's live location, so a silently-accepted MITM certificate
       would be worse than a report that fails loudly and gets logged. */
    if (ca_cert_path == NULL || ca_cert_path[0] == '\0') {
        set_err(err_buf, err_buf_size, "no ca_cert_path configured - refusing to connect without verification");
        goto cleanup;
    }

    ret = mbedtls_x509_crt_parse_file(&ca_cert, ca_cert_path);
    if (ret != 0) {
        set_err_code(err_buf, err_buf_size, "could not load ca_cert_path", ret);
        goto cleanup;
    }

    ret = mbedtls_net_connect(&server_fd, host, port, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        set_err_code(err_buf, err_buf_size, "net_connect failed", ret);
        goto cleanup;
    }
    mbedtls_net_set_block(&server_fd);

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        set_err_code(err_buf, err_buf_size, "ssl_config_defaults failed", ret);
        goto cleanup;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &ca_cert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_read_timeout(&conf, IO_TIMEOUT_MS);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        set_err_code(err_buf, err_buf_size, "ssl_setup failed", ret);
        goto cleanup;
    }

    ret = mbedtls_ssl_set_hostname(&ssl, host);
    if (ret != 0) {
        set_err_code(err_buf, err_buf_size, "ssl_set_hostname failed", ret);
        goto cleanup;
    }

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            set_err_code(err_buf, err_buf_size, "TLS handshake failed", ret);
            goto cleanup;
        }
    }

    uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
    if (flags != 0) {
        char vrfy_buf[256];
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "", flags);
        char full[300];
        snprintf(full, sizeof(full), "certificate verification failed: %s", vrfy_buf);
        set_err(err_buf, err_buf_size, full);
        goto cleanup;
    }

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
        set_err(err_buf, err_buf_size, "request too large for internal buffer");
        goto cleanup;
    }

    size_t sent_total = 0;
    while (sent_total < (size_t)req_len) {
        ret = mbedtls_ssl_write(&ssl, (const unsigned char *)request + sent_total,
                                 (size_t)req_len - sent_total);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret < 0) {
            set_err_code(err_buf, err_buf_size, "ssl_write failed", ret);
            goto cleanup;
        }
        sent_total += (size_t)ret;
    }

    char response[4096];
    size_t resp_len = 0;
    for (;;) {
        if (resp_len >= sizeof(response) - 1) break;
        ret = mbedtls_ssl_read(&ssl, (unsigned char *)response + resp_len,
                                sizeof(response) - 1 - resp_len);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) break;
        if (ret < 0) {
            set_err_code(err_buf, err_buf_size, "ssl_read failed", ret);
            goto cleanup;
        }
        resp_len += (size_t)ret;
    }
    response[resp_len] = '\0';

    if (resp_len == 0) {
        set_err(err_buf, err_buf_size, "no response received");
        goto cleanup;
    }

    {
        int status = 0;
        if (sscanf(response, "HTTP/%*d.%*d %d", &status) != 1) {
            set_err(err_buf, err_buf_size, "could not parse HTTP status line");
            goto cleanup;
        }
        if (status_out != NULL) *status_out = status;
        if (status < 200 || status >= 300) {
            set_err(err_buf, err_buf_size, "non-2xx HTTP status");
            goto cleanup;
        }
    }

    rc = 0;

cleanup:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    mbedtls_x509_crt_free(&ca_cert);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return rc;
}
