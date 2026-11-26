#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void config_load_defaults(config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    /* On-device this should be overridden to something under
       /sdcard/Documents/calculator-companion/ by the real on-device config
       file. The built-in default below is a safe, generic fallback. */
    strncpy(cfg->log_path, "./calculator-companion/daemon.log", CFG_MAX_PATH - 1);
    strncpy(cfg->baseline_path, "./calculator-companion/sim_baseline.txt", CFG_MAX_PATH - 1);
    cfg->test_mode = 1; /* safe by default: no real action until explicitly turned off */
    cfg->poll_interval_sec = 30;
}

static void trim(char *s) {
    /* trim trailing newline/whitespace */
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

int config_load_file(config_t *cfg, const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        const char *key = line;
        const char *value = eq + 1;

        if (strcmp(key, "log_path") == 0) {
            strncpy(cfg->log_path, value, CFG_MAX_PATH - 1);
        } else if (strcmp(key, "baseline_path") == 0) {
            strncpy(cfg->baseline_path, value, CFG_MAX_PATH - 1);
        } else if (strcmp(key, "test_mode") == 0) {
            cfg->test_mode = atoi(value);
        } else if (strcmp(key, "poll_interval_sec") == 0) {
            int v = atoi(value);
            if (v > 0) cfg->poll_interval_sec = v;
        }
        /* unknown keys are silently ignored, forward-compatible */
    }

    fclose(fp);
    return 0;
}
