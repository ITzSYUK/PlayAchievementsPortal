#ifndef RA_HTTP_BODY_H
#define RA_HTTP_BODY_H
#include <stddef.h>
#include <stdint.h>
#include "ra_response_limits.h"
#define RA_HTTP_HEADER_LIMIT 4096u
#define RA_HTTP_TRAILER_LIMIT 2048u
#define RA_HTTP_CHUNK_META_LIMIT 16384u
enum {
    RA_HTTP_STATUS, RA_HTTP_HEADER, RA_HTTP_LENGTH, RA_HTTP_CLOSE,
    RA_HTTP_CHUNK_SIZE, RA_HTTP_CHUNK, RA_HTTP_CHUNK_CR, RA_HTTP_CHUNK_LF,
    RA_HTTP_TRAILER, RA_HTTP_DONE
};
enum {
    RA_HTTP_OK, RA_HTTP_INPUT, RA_HTTP_BAD_STATUS, RA_HTTP_BAD_HEADER,
    RA_HTTP_HEADERS_TOO_LARGE, RA_HTTP_LINE_TOO_LONG, RA_HTTP_BAD_LENGTH,
    RA_HTTP_NUMBER_OVERFLOW, RA_HTTP_BODY_TOO_LARGE, RA_HTTP_AMBIGUOUS,
    RA_HTTP_BAD_TRANSFER_ENCODING, RA_HTTP_BAD_CONTENT_ENCODING,
    RA_HTTP_BAD_CHUNK, RA_HTTP_CHUNK_META_TOO_LARGE, RA_HTTP_BAD_TRAILER,
    RA_HTTP_TRAILERS_TOO_LARGE, RA_HTTP_TRAILER_FRAMING, RA_HTTP_BAD_CRLF,
    RA_HTTP_TRUNCATED, RA_HTTP_EXTRA_DATA, RA_HTTP_UNAUTHENTICATED_CLOSE, RA_HTTP_TRANSPORT_ERROR
};
enum { RA_HTTP_ENCODING_ABSENT, RA_HTTP_ENCODING_IDENTITY, RA_HTTP_ENCODING_GZIP,
       RA_HTTP_ENCODING_DEFLATE, RA_HTTP_ENCODING_BR, RA_HTTP_ENCODING_OTHER };
typedef struct {
    unsigned char* body;
    size_t capacity, size, remaining, header_bytes, line_size;
    size_t trailer_bytes, chunk_meta_bytes;
    uint32_t declared_length, last_chunk, chunk_count;
    int phase, status, has_length, chunked, content_encoding, length_overflow, error;
    char line[1024];
} ra_http_body;
void ra_http_body_init(ra_http_body*, unsigned char*, size_t);
/* Returns -1 on invalid/oversized framing, 0 incomplete, 1 complete. */
int ra_http_body_feed(ra_http_body*, const unsigned char*, size_t);
int ra_http_body_eof(ra_http_body*);
/* Fixed strings only; no server-controlled content may reach logs. */
const char* ra_http_error_name(int);
const char* ra_http_phase_name(int);
const char* ra_http_encoding_name(int);
#endif
