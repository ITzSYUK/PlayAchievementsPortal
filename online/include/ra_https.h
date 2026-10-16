#ifndef RA_HTTPS_H
#define RA_HTTPS_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "ra_response_limits.h"
#define RA_HTTPS_INVALID (-35201)
#define RA_HTTPS_LIMIT (-35202)
#define RA_HTTPS_TIMEOUT (-35203)
#define RA_HTTPS_CANCEL (-35204)
#define RA_HTTPS_FRAMING (-35205)
#define RA_HTTPS_ENCODING (-35206)
#define RA_HTTPS_TRUNCATED (-35207)
#define RA_HTTPS_HEAP_LIMIT 65536u
/* One transport owner. No request/response logging. BIO is nonblocking.
 * TLS 1.2, required chain/hostname/time verification, no redirects. */
typedef struct {
    void* context;
    int (*send)(void*, const unsigned char*, size_t);
    int (*recv)(void*, unsigned char*, size_t);
    int (*wait)(void*, int, unsigned);
    int (*entropy)(void*, unsigned char*, size_t);
    uint64_t (*now_ms)(void*);
    time_t (*utc)(void*);
    int (*cancelled)(void*);
    const unsigned char* ca_der;
    size_t ca_size;
    const char* hostname;
    const char* post;
    size_t post_size;
    unsigned char* body;
    size_t capacity;
    uint64_t deadline_ms;
} ra_https_options;
typedef struct {
    int code, status;
    const char* stage;
    uint32_t verify_flags;
    size_t body_size, heap_peak, heap_remaining;
    /* Diagnostics survive rejection/zeroing. body_size stays 0 on failure. */
    size_t body_received, wire_bytes, header_bytes, trailer_bytes, chunk_meta_bytes, line_bytes;
    uint32_t declared_length, last_chunk, chunk_count;
    int http_error, http_phase, has_length, chunked, content_encoding, length_overflow;
} ra_https_result;
int ra_https_post(const ra_https_options*, ra_https_result*);
#endif
