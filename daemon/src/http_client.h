#ifndef GUARD_HTTP_CLIENT_H
#define GUARD_HTTP_CLIENT_H

#include <stddef.h>

/* Minimal blocking HTTP/1.1 client: POSTs body (as application/json) to
   url. Only http:// is implemented here - https:// (TLS) is a deliberate
   follow-up step, done sequentially as agreed, not bundled into this one.
   On failure, writes a short human-readable reason into err_buf (if
   non-NULL) and returns -1. On success (2xx response), returns 0 and, if
   status_out is non-NULL, writes the numeric HTTP status code into it.
   Bounded by internal connect/IO timeouts - never hangs indefinitely. */
int http_post(const char *url, const char *body, int *status_out,
              char *err_buf, size_t err_buf_size);

#endif
