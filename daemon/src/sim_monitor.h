#ifndef GUARD_SIM_MONITOR_H
#define GUARD_SIM_MONITOR_H

#include "config.h"

#define SIM_OPERATOR_MAX 32

typedef struct {
    char operator_numeric[SIM_OPERATOR_MAX]; /* e.g. "43408", empty if unavailable */
    int  roaming;                             /* 1 = roaming, 0 = not roaming/unknown */
} sim_identity_t;

/* Reads the current SIM identity from the system (via getprop). Always
   fills out, even on partial failure (fields are left empty/0). */
void sim_monitor_get_current(sim_identity_t *out);

/* Loads the previously-saved baseline identity from cfg->baseline_path.
   Returns 1 if a baseline was found and loaded, 0 if none exists yet. */
int sim_monitor_load_baseline(const config_t *cfg, sim_identity_t *out);

/* Persists ident as the new baseline at cfg->baseline_path.
   Returns 0 on success, -1 on failure. */
int sim_monitor_save_baseline(const config_t *cfg, const sim_identity_t *ident);

/* 1 if a and b represent the same SIM (by operator numeric), else 0. */
int sim_monitor_same(const sim_identity_t *a, const sim_identity_t *b);

#endif
