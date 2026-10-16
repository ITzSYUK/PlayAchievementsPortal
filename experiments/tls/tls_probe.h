#ifndef RA_TLS_PROBE_H
#define RA_TLS_PROBE_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "probe_log.h"

#define RA_TLS_TIMEOUT (-31001)
#define RA_TLS_HEADER_LIMIT (-31002)
#define RA_TLS_BAD_HTTP (-31003)
#define RA_TLS_CA_LIMIT (-31004)
#define RA_TLS_CLOCK_ERROR (-31005)
#define RA_TLS_MEMORY_LIMIT (2u * 1024u * 1024u)

/* BIO callbacks are nonblocking; return Mbed TLS WANT_READ/WRITE as needed.
 * wait_io must have a bounded wait, using the same monotonic clock as now_ms.
 * Single owner: no simultaneous invocations or allocator use on other threads. */
typedef struct {
    int (*send)(void*, const unsigned char*, size_t);
    int (*recv)(void*, unsigned char*, size_t);
    int (*wait_io)(void*, int writing, unsigned timeout_ms);
    int (*entropy)(void*, unsigned char*, size_t);
    uint64_t (*now_ms)(void*);
    /* UTC Unix seconds for X.509; separate from the monotonic deadline clock.
     * Return (time_t)-1 if the source fails, never fall back to uptime. */
    time_t (*utc_time)(void*);
    void* io;
    void* random;
    const char* hostname;
    const char* path;
    const char* ca_file;
    unsigned timeout_ms;
    ra_probe_log* log;
} ra_tls_options;
typedef struct {
    int code;
    const char* stage;
    uint32_t verify_flags;
    int http_status;
    unsigned sample_bytes;
    size_t memory_peak;
    size_t memory_remaining;
    const char* protocol;
    const char* cipher;
} ra_tls_result;
int ra_tls_probe(const ra_tls_options* options, ra_tls_result* result);
#endif
