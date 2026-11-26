#include "report.h"
#include "log.h"
#include "http_client.h"

#include <stdio.h>

void report_sim_event(const config_t *cfg, const sim_identity_t *baseline,
                       const sim_identity_t *current) {
    if (cfg->server_count == 0) {
        log_write(LOG_WARN, "report_sim_event: no server_urls configured, nothing to report to");
        return;
    }

    char body[256];
    snprintf(body, sizeof(body),
             "{\"event\":\"sim_changed\",\"baseline\":\"%s\",\"current\":\"%s\",\"roaming\":%s}",
             baseline->operator_numeric, current->operator_numeric,
             current->roaming ? "true" : "false");

    for (int i = 0; i < cfg->server_count; i++) {
        if (cfg->test_mode) {
            log_write(LOG_INFO, "[TEST_MODE] would POST to %s: %s", cfg->server_urls[i], body);
            continue;
        }

        int status = 0;
        char err[128] = {0};
        if (http_post(cfg->server_urls[i], body, &status, err, sizeof(err)) == 0) {
            log_write(LOG_INFO, "[LIVE] reported to %s (HTTP %d)", cfg->server_urls[i], status);
        } else {
            log_write(LOG_ERROR, "[LIVE] report to %s failed: %s", cfg->server_urls[i], err);
        }
    }
}
