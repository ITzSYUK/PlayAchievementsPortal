#include "probe_log.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct sink {
    char data[384];
    unsigned length;
    unsigned calls;
    int mode;
} sink;

static int writer(const void* data, unsigned size, void* userdata)
{
    sink* output = (sink*)userdata;
    ++output->calls;
    if (output->mode == 1 && output->calls > 1)
        return -77;
    if (output->mode == 2)
        return 0;
    if (output->mode == 3)
        return (int)size + 1;
    /* Deliberately partial writes catch the silent truncation failure mode. */
    if (size > 3)
        size = 3;
    memcpy(output->data + output->length, data, size);
    output->length += size;
    return (int)size;
}

int main(void)
{
    sink output = {{0}, 0, 0, 0};
    ra_probe_log log;
    unsigned calls;
    char oversized[400];
    ra_probe_log_init(&log, writer, &output);
    CHECK(ra_probe_log_printf(&log, "state=%d code=0x%08X\n", 4, 0x80431075u));
    CHECK(strcmp(output.data, "state=4 code=0x80431075\n") == 0);
    CHECK(output.calls > 1 && log.error == 0);

    memset(&output, 0, sizeof(output));
    output.mode = 1;
    ra_probe_log_init(&log, writer, &output);
    CHECK(!ra_probe_log_printf(&log, "a full line\n"));
    CHECK(log.error == -77 && output.length == 3);
    calls = output.calls;
    CHECK(!ra_probe_log_printf(&log, "retry must not hide the first error\n"));
    CHECK(log.error == -77 && output.calls == calls);

    output.mode = 2;
    ra_probe_log_init(&log, writer, &output);
    CHECK(!ra_probe_log_printf(&log, "zero write\n"));
    CHECK(log.error == RA_PROBE_LOG_INVALID_WRITE);
    output.mode = 3;
    ra_probe_log_init(&log, writer, &output);
    CHECK(!ra_probe_log_printf(&log, "invalid count\n"));
    CHECK(log.error == RA_PROBE_LOG_INVALID_WRITE);

    memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized) - 1] = 0;
    calls = output.calls;
    ra_probe_log_init(&log, writer, &output);
    CHECK(!ra_probe_log_printf(&log, "%s", oversized));
    CHECK(log.error == RA_PROBE_LOG_LINE_TOO_LONG && output.calls == calls);
    puts("PASS diagnostic log: full partial writes, write failures, zero/invalid counts and oversized lines");
    return 0;
}
