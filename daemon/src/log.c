#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

static FILE *g_log_fp = NULL;

static const char *level_name(log_level_t level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        default:        return "?";
    }
}

/* Creates every directory component of path (best-effort, like mkdir -p). */
static void mkdirs(const char *path) {
    char buf[1024];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(buf)) return;
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0755); /* ignore EEXIST and other errors here */
            *p = '/';
        }
    }
    mkdir(buf, 0755); /* the final component itself has no trailing '/' to trigger on */
}

int log_init(const char *path) {
    if (path == NULL) {
        g_log_fp = stderr;
        return -1;
    }

    /* Split off the directory portion and ensure it exists. */
    char dirbuf[1024];
    strncpy(dirbuf, path, sizeof(dirbuf) - 1);
    dirbuf[sizeof(dirbuf) - 1] = '\0';
    char *slash = strrchr(dirbuf, '/');
    if (slash != NULL) {
        *slash = '\0';
        mkdirs(dirbuf);
    }

    g_log_fp = fopen(path, "a");
    if (g_log_fp == NULL) {
        g_log_fp = stderr;
        return -1;
    }
    setvbuf(g_log_fp, NULL, _IOLBF, 0); /* line-buffered: survive a crash */
    return 0;
}

void log_write(log_level_t level, const char *fmt, ...) {
    FILE *fp = g_log_fp ? g_log_fp : stderr;

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_now);

    fprintf(fp, "%s [%s] ", timebuf, level_name(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fprintf(fp, "\n");
    fflush(fp);
}

void log_close(void) {
    if (g_log_fp != NULL && g_log_fp != stderr) {
        fclose(g_log_fp);
    }
    g_log_fp = NULL;
}
