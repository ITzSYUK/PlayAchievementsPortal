#include "probe_log.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

void ra_probe_log_init(ra_probe_log* log, ra_probe_log_write_t write, void* userdata)
{
    log->write = write;
    log->userdata = userdata;
    log->error = 0;
}

int ra_probe_log_printf(ra_probe_log* log, const char* format, ...)
{
    char line[384];
    va_list args;
    int length;
    unsigned offset = 0;

    if (log == NULL)
        return 0;
    if (log->error != 0)
        return 0;
    if (log->write == NULL || format == NULL) {
        log->error = RA_PROBE_LOG_FORMAT_ERROR;
        return 0;
    }
    va_start(args, format);
    length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length < 0 || (unsigned)length >= sizeof(line)) {
        log->error = length < 0 ? RA_PROBE_LOG_FORMAT_ERROR : RA_PROBE_LOG_LINE_TOO_LONG;
        return 0;
    }
    while (offset < (unsigned)length) {
        const unsigned remaining = (unsigned)length - offset;
        const int written = log->write(line + offset, remaining, log->userdata);
        if (written <= 0 || (unsigned)written > remaining) {
            log->error = written < 0 ? written : RA_PROBE_LOG_INVALID_WRITE;
            return 0;
        }
        offset += (unsigned)written;
    }
    return 1;
}
