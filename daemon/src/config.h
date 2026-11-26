#ifndef GUARD_CONFIG_H
#define GUARD_CONFIG_H

#define CFG_MAX_PATH 512
#define CFG_MAX_SERVERS 8
#define CFG_MAX_URL 256

typedef struct {
    char log_path[CFG_MAX_PATH];      /* on-device default: /sdcard/Documents/calculator-companion/daemon.log */
    char baseline_path[CFG_MAX_PATH]; /* where the "known good" SIM identity is stored */
    int  test_mode;                   /* 1 = no real actions, only log what would happen */
    int  poll_interval_sec;           /* how often the main loop re-checks SIM state */

    char server_urls[CFG_MAX_SERVERS][CFG_MAX_URL]; /* report destinations, sent in parallel */
    int  server_count;

    char ca_cert_path[CFG_MAX_PATH]; /* PEM file to trust for https:// servers; required for https */
} config_t;

/* Fills cfg with built-in defaults. Always call this before config_load_file. */
void config_load_defaults(config_t *cfg);

/* Loads simple "key=value" lines from path, overriding matching defaults.
   Missing file or unknown keys are not errors. Returns 0 on success,
   -1 if the file could not be opened (cfg is left at its current values). */
int config_load_file(config_t *cfg, const char *path);

#endif
