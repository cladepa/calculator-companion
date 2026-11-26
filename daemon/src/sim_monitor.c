#include "sim_monitor.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Creates every directory component of path (best-effort, like mkdir -p). */
static void mkdirs(const char *path) {
    char buf[CFG_MAX_PATH];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(buf)) return;
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0755);
            *p = '/';
        }
    }
    mkdir(buf, 0755); /* the final component itself has no trailing '/' to trigger on */
}

/* Runs `getprop <prop_name>` and copies the (trimmed) first line of output
   into out (size outsz). Leaves out empty on any failure. This shells out
   to the system's own getprop rather than linking against bionic directly,
   so the daemon has no Android-version-specific library dependency at all -
   in tests, a fake "getprop" earlier in PATH stands in for the real one. */
static void run_getprop(const char *prop_name, char *out, size_t outsz) {
    out[0] = '\0';
    if (outsz == 0) return;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", prop_name);

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        log_write(LOG_ERROR, "sim_monitor: popen(getprop %s) failed", prop_name);
        return;
    }

    if (fgets(out, (int)outsz, fp) != NULL) {
        size_t len = strlen(out);
        while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) {
            out[--len] = '\0';
        }
    }
    pclose(fp);
}

void sim_monitor_get_current(sim_identity_t *out) {
    memset(out, 0, sizeof(*out));

    run_getprop("gsm.sim.operator.numeric", out->operator_numeric,
                sizeof(out->operator_numeric));

    char roaming_buf[32];
    /* gsm.operator.isroaming on dual-SIM devices can read like "false,false";
       treat any "true" substring as roaming. */
    run_getprop("gsm.operator.isroaming", roaming_buf, sizeof(roaming_buf));
    out->roaming = (strstr(roaming_buf, "true") != NULL) ? 1 : 0;
}

int sim_monitor_load_baseline(const config_t *cfg, sim_identity_t *out) {
    memset(out, 0, sizeof(*out));
    FILE *fp = fopen(cfg->baseline_path, "r");
    if (fp == NULL) return 0;

    char line[128];
    if (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        strncpy(out->operator_numeric, line, sizeof(out->operator_numeric) - 1);
        out->operator_numeric[sizeof(out->operator_numeric) - 1] = '\0';
    }
    fclose(fp);
    return out->operator_numeric[0] != '\0';
}

int sim_monitor_save_baseline(const config_t *cfg, const sim_identity_t *ident) {
    char dirbuf[CFG_MAX_PATH];
    strncpy(dirbuf, cfg->baseline_path, sizeof(dirbuf) - 1);
    dirbuf[sizeof(dirbuf) - 1] = '\0';
    char *slash = strrchr(dirbuf, '/');
    if (slash != NULL) {
        *slash = '\0';
        mkdirs(dirbuf);
    }

    FILE *fp = fopen(cfg->baseline_path, "w");
    if (fp == NULL) {
        log_write(LOG_ERROR, "sim_monitor: cannot write baseline to %s", cfg->baseline_path);
        return -1;
    }
    fprintf(fp, "%s\n", ident->operator_numeric);
    fclose(fp);
    return 0;
}

int sim_monitor_same(const sim_identity_t *a, const sim_identity_t *b) {
    return strcmp(a->operator_numeric, b->operator_numeric) == 0;
}
