#ifndef PROBE_LOG_H
#define PROBE_LOG_H

typedef int (*ra_probe_log_write_t)(const void* data, unsigned size, void* userdata);

typedef struct ra_probe_log {
    ra_probe_log_write_t write;
    void* userdata;
    int error;
} ra_probe_log;

enum {
    RA_PROBE_LOG_FORMAT_ERROR = -32001,
    RA_PROBE_LOG_LINE_TOO_LONG = -32002,
    RA_PROBE_LOG_INVALID_WRITE = -32003
};

void ra_probe_log_init(ra_probe_log* log, ra_probe_log_write_t write, void* userdata);
/* Writes immediately; preserves the first error and never reports a partial
 * or truncated line as successful. Intended for a single owning thread. */
int ra_probe_log_printf(ra_probe_log* log, const char* format, ...);

#endif
