#ifndef GUARD_HTTPS_CLIENT_H
#define GUARD_HTTPS_CLIENT_H

#include <stddef.h>

/* Like http_post, but for https:// URLs, with full certificate and
   hostname verification via mbedTLS. ca_cert_path must point to a PEM
   file containing the CA (or the server's own self-signed certificate)
   to trust - there is deliberately no "skip verification" mode, since
   this channel carries a victim's location data and must not be
   interceptable by a man in the middle. Returns 0 on success (2xx
   response), -1 on any failure, with a short reason in err_buf. */
int https_post(const char *url, const char *body, const char *ca_cert_path,
                int *status_out, char *err_buf, size_t err_buf_size);

#endif
