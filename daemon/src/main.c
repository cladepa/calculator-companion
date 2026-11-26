#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "config.h"
#include "sim_monitor.h"

int main(int argc, char **argv) {
    config_t cfg;
    config_load_defaults(&cfg);

    const char *config_path = "config.txt";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        }
    }
    config_load_file(&cfg, config_path); /* missing file is fine, defaults stand */

    log_init(cfg.log_path);
    log_write(LOG_INFO, "daemon starting (test_mode=%d poll_interval_sec=%d)",
               cfg.test_mode, cfg.poll_interval_sec);

    sim_identity_t baseline;
    if (!sim_monitor_load_baseline(&cfg, &baseline)) {
        sim_monitor_get_current(&baseline);
        sim_monitor_save_baseline(&cfg, &baseline);
        log_write(LOG_INFO, "no baseline found - adopting current SIM as baseline (operator=%s roaming=%d)",
                   baseline.operator_numeric, baseline.roaming);
    } else {
        log_write(LOG_INFO, "loaded baseline SIM (operator=%s)", baseline.operator_numeric);
    }

    int run_once = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--once") == 0) run_once = 1;
    }

    do {
        sim_identity_t current;
        sim_monitor_get_current(&current);

        if (!sim_monitor_same(&current, &baseline)) {
            log_write(LOG_WARN, "SIM CHANGED: baseline=%s current=%s roaming=%d",
                       baseline.operator_numeric, current.operator_numeric, current.roaming);
            if (cfg.test_mode) {
                log_write(LOG_INFO, "[TEST_MODE] would start decoy-window escalation sequence now (no real action taken)");
            } else {
                log_write(LOG_WARN, "[LIVE] escalation sequence not implemented yet - no action taken");
            }
        } else {
            log_write(LOG_DEBUG, "sim unchanged (operator=%s)", current.operator_numeric);
        }

        if (run_once) break;
        sleep((unsigned int)cfg.poll_interval_sec);
    } while (1);

    log_write(LOG_INFO, "daemon exiting");
    log_close();
    return 0;
}
