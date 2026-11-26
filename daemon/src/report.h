#ifndef GUARD_REPORT_H
#define GUARD_REPORT_H

#include "config.h"
#include "sim_monitor.h"

/* Sends (or, in TEST_MODE, just logs) a report describing the current SIM
   state to every server address configured. Real network code comes later;
   for now this only validates the data flow end-to-end and is always safe
   to call, since cfg->test_mode gates every externally-visible action. */
void report_sim_event(const config_t *cfg, const sim_identity_t *baseline,
                       const sim_identity_t *current);

#endif
