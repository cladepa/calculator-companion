#include "report.h"
#include "log.h"

#include <stdio.h>

void report_sim_event(const config_t *cfg, const sim_identity_t *baseline,
                       const sim_identity_t *current) {
    if (cfg->server_count == 0) {
        log_write(LOG_WARN, "report_sim_event: no server_urls configured, nothing to report to");
        return;
    }

    for (int i = 0; i < cfg->server_count; i++) {
        if (cfg->test_mode) {
            log_write(LOG_INFO,
                      "[TEST_MODE] would POST to %s: {\"event\":\"sim_changed\",\"baseline\":\"%s\",\"current\":\"%s\",\"roaming\":%s}",
                      cfg->server_urls[i], baseline->operator_numeric,
                      current->operator_numeric, current->roaming ? "true" : "false");
        } else {
            /* Real HTTP(S) POST not implemented yet - this is the next
               piece to build (minimal client, no dependency on system
               curl/wget per our earlier decision). */
            log_write(LOG_ERROR,
                      "[LIVE] real network send to %s not implemented yet - report dropped",
                      cfg->server_urls[i]);
        }
    }
}
