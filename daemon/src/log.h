#ifndef GUARD_LOG_H
#define GUARD_LOG_H

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} log_level_t;

/* Opens (creating parent dirs as needed) the log file at path for appending.
   Returns 0 on success, -1 on failure (falls back to stderr). */
int log_init(const char *path);

/* Writes a single timestamped, leveled log line, printf-style. */
void log_write(log_level_t level, const char *fmt, ...);

void log_close(void);

#endif
