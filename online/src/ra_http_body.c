#include "ra_http_body.h"
#include <string.h>

static int fail(ra_http_body* d, int error)
{ if (!d->error) d->error = error; return -1; }
static int equal(const char* a, const char* b)
{
    while (*a && *b) {
        unsigned ch = (unsigned char)*a++;
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        if (ch != (unsigned char)*b++) return 0;
    }
    return !*a && !*b;
}
static int token(unsigned ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') || (ch && strchr("!#$%&'*+-.^_`|~", (int)ch));
}
static const char* whitespace(const char* p)
{ while (*p == ' ' || *p == '\t') ++p; return p; }
/* RFC 9112 7.1.1: validate and ignore bounded extensions, including quoted
 * strings/quoted-pairs. No extension content is retained or logged. */
static int extensions(const char* p)
{
    while (*p) {
        const char* start;
        p = whitespace(p); if (!*p) break;
        if (*p++ != ';') return 0;
        p = whitespace(p); start = p;
        while (token((unsigned char)*p)) ++p;
        if (p == start) return 0;
        p = whitespace(p);
        if (*p == '=') {
            p = whitespace(p + 1);
            if (*p == '"') {
                ++p;
                while (*p && *p != '"') {
                    if (*p == '\\' && !*++p) return 0;
                    ++p;
                }
                if (*p != '"') return 0;
                ++p;
            } else {
                start = p; while (token((unsigned char)*p)) ++p;
                if (p == start) return 0;
            }
        }
    }
    return 1;
}
static int chunk_line(ra_http_body* d)
{
    uint32_t number = 0;
    const char* p = d->line;
    unsigned digits = 0;
    while (*p) {
        unsigned ch = (unsigned char)*p, digit;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
        else break;
        if (number > (UINT32_MAX - digit) / 16) {
            d->last_chunk = UINT32_MAX; d->length_overflow = 1;
            return fail(d, RA_HTTP_NUMBER_OVERFLOW);
        }
        number = number * 16 + digit; ++digits; ++p;
    }
    d->last_chunk = number;
    if (!digits || !extensions(p)) return fail(d, RA_HTTP_BAD_CHUNK);
    if (number > d->capacity - d->size) return fail(d, RA_HTTP_BODY_TOO_LARGE);
    d->remaining = number;
    if (number) { ++d->chunk_count; d->phase = RA_HTTP_CHUNK; }
    else d->phase = RA_HTTP_TRAILER;
    return 0;
}
static int line(ra_http_body* d)
{
    char* value;
    size_t i;
    if (d->phase == RA_HTTP_STATUS) {
        if (d->line_size < 12 || (strncmp(d->line, "HTTP/1.1 ", 9) && strncmp(d->line, "HTTP/1.0 ", 9)) ||
            d->line[9] < '2' || d->line[9] > '5' || d->line[10] < '0' || d->line[10] > '9' ||
            d->line[11] < '0' || d->line[11] > '9' || (d->line[12] && d->line[12] != ' '))
            return fail(d, RA_HTTP_BAD_STATUS);
        d->status = (d->line[9]-'0')*100+(d->line[10]-'0')*10+d->line[11]-'0';
        d->phase = RA_HTTP_HEADER; return 0;
    }
    if (d->phase == RA_HTTP_CHUNK_SIZE) return chunk_line(d);
    if (!d->line_size) {
        if (d->phase == RA_HTTP_TRAILER) { d->phase = RA_HTTP_DONE; return 0; }
        if (d->has_length && d->chunked) return fail(d, RA_HTTP_AMBIGUOUS);
        d->phase = d->chunked ? RA_HTTP_CHUNK_SIZE : d->has_length ?
                   (d->remaining ? RA_HTTP_LENGTH : RA_HTTP_DONE) : RA_HTTP_CLOSE;
        return 0;
    }
    value = strchr(d->line, ':');
    if (!value || value == d->line)
        return fail(d, d->phase == RA_HTTP_TRAILER ? RA_HTTP_BAD_TRAILER : RA_HTTP_BAD_HEADER);
    for (i = 0; d->line + i < value; ++i)
        if (!token((unsigned char)d->line[i]))
            return fail(d, d->phase == RA_HTTP_TRAILER ? RA_HTTP_BAD_TRAILER : RA_HTTP_BAD_HEADER);
    *value++ = 0;
    while (*value == ' ' || *value == '\t') ++value;
    i = strlen(value); while (i && (value[i-1] == ' ' || value[i-1] == '\t')) value[--i] = 0;
    if (d->phase == RA_HTTP_TRAILER) {
        /* Never merge trailing fields into the framing/encoding headers. */
        if (equal(d->line, "content-length") || equal(d->line, "transfer-encoding") ||
            equal(d->line, "content-encoding")) return fail(d, RA_HTTP_TRAILER_FRAMING);
        return 0;
    }
    if (equal(d->line, "content-length")) {
        uint32_t number = 0;
        if (d->has_length) return fail(d, RA_HTTP_AMBIGUOUS);
        d->has_length = 1;
        if (!*value) return fail(d, RA_HTTP_BAD_LENGTH);
        for (i = 0; value[i]; ++i) {
            unsigned digit = (unsigned char)value[i] - '0';
            if (digit > 9) return fail(d, RA_HTTP_BAD_LENGTH);
            if (number > (UINT32_MAX - digit) / 10) {
                d->declared_length = UINT32_MAX; d->length_overflow = 1;
                return fail(d, RA_HTTP_NUMBER_OVERFLOW);
            }
            number = number * 10 + digit;
        }
        d->declared_length = number;
        if (number > d->capacity) return fail(d, RA_HTTP_BODY_TOO_LARGE);
        d->remaining = number;
    } else if (equal(d->line, "transfer-encoding")) {
        if (d->chunked) return fail(d, RA_HTTP_AMBIGUOUS);
        if (!equal(value, "chunked")) return fail(d, RA_HTTP_BAD_TRANSFER_ENCODING);
        d->chunked = 1;
    } else if (equal(d->line, "content-encoding")) {
        if (d->content_encoding) return fail(d, RA_HTTP_AMBIGUOUS);
        d->content_encoding = equal(value, "identity") ? RA_HTTP_ENCODING_IDENTITY :
            equal(value, "gzip") ? RA_HTTP_ENCODING_GZIP : equal(value, "deflate") ? RA_HTTP_ENCODING_DEFLATE :
            equal(value, "br") ? RA_HTTP_ENCODING_BR : RA_HTTP_ENCODING_OTHER;
        if (d->content_encoding != RA_HTTP_ENCODING_IDENTITY) return fail(d, RA_HTTP_BAD_CONTENT_ENCODING);
    }
    return 0;
}
void ra_http_body_init(ra_http_body* d, unsigned char* body, size_t capacity)
{
    memset(d, 0, sizeof(*d)); d->body = body; d->capacity = capacity;
    if (!body || !capacity || capacity > RA_RESPONSE_BODY_LIMIT) d->error = RA_HTTP_INPUT;
}
int ra_http_body_feed(ra_http_body* d, const unsigned char* bytes, size_t size)
{
    size_t i;
    if (d->error) return -1;
    if (!bytes && size) return fail(d, RA_HTTP_INPUT);
    for (i = 0; i < size; ++i) {
        unsigned ch = bytes[i];
        if (d->phase == RA_HTTP_DONE) return fail(d, RA_HTTP_EXTRA_DATA);
        if (d->phase == RA_HTTP_LENGTH || d->phase == RA_HTTP_CLOSE || d->phase == RA_HTTP_CHUNK) {
            if (d->size == d->capacity) return fail(d, RA_HTTP_BODY_TOO_LARGE);
            d->body[d->size++] = (unsigned char)ch;
            if (d->phase != RA_HTTP_CLOSE && --d->remaining == 0)
                d->phase = d->phase == RA_HTTP_CHUNK ? RA_HTTP_CHUNK_CR : RA_HTTP_DONE;
        } else if (d->phase == RA_HTTP_CHUNK_CR || d->phase == RA_HTTP_CHUNK_LF) {
            if (++d->chunk_meta_bytes > RA_HTTP_CHUNK_META_LIMIT) return fail(d, RA_HTTP_CHUNK_META_TOO_LARGE);
            if (ch != (d->phase == RA_HTTP_CHUNK_CR ? '\r' : '\n')) return fail(d, RA_HTTP_BAD_CRLF);
            d->phase = d->phase == RA_HTTP_CHUNK_CR ? RA_HTTP_CHUNK_LF : RA_HTTP_CHUNK_SIZE;
        } else {
            if (d->phase == RA_HTTP_CHUNK_SIZE) {
                if (++d->chunk_meta_bytes > RA_HTTP_CHUNK_META_LIMIT) return fail(d, RA_HTTP_CHUNK_META_TOO_LARGE);
            } else if (d->phase == RA_HTTP_TRAILER) {
                if (++d->trailer_bytes > RA_HTTP_TRAILER_LIMIT) return fail(d, RA_HTTP_TRAILERS_TOO_LARGE);
            } else if (++d->header_bytes > RA_HTTP_HEADER_LIMIT) return fail(d, RA_HTTP_HEADERS_TOO_LARGE);
            if (!ch || ch == 127 || (ch < 32 && ch != '\r' && ch != '\n' && ch != '\t'))
                return fail(d, d->phase == RA_HTTP_CHUNK_SIZE ? RA_HTTP_BAD_CHUNK :
                               d->phase == RA_HTTP_TRAILER ? RA_HTTP_BAD_TRAILER : RA_HTTP_BAD_HEADER);
            if (ch == '\n') {
                if (!d->line_size || d->line[d->line_size-1] != '\r') return fail(d, RA_HTTP_BAD_CRLF);
                d->line[--d->line_size] = 0;
                if (line(d)) return -1;
                d->line_size = 0;
            } else {
                if (d->line_size >= sizeof(d->line)-1) return fail(d, RA_HTTP_LINE_TOO_LONG);
                if (d->line_size && d->line[d->line_size-1] == '\r') return fail(d, RA_HTTP_BAD_CRLF);
                d->line[d->line_size++] = (char)ch;
            }
        }
    }
    return d->phase == RA_HTTP_DONE;
}
int ra_http_body_eof(ra_http_body* d)
{
    if (d->error) return -1;
    if (d->phase == RA_HTTP_CLOSE) d->phase = RA_HTTP_DONE;
    return d->phase == RA_HTTP_DONE ? 1 : fail(d, RA_HTTP_TRUNCATED);
}
const char* ra_http_error_name(int error)
{
    static const char* const names[] = { "none", "input", "status", "header_syntax",
        "header_limit", "line_limit", "content_length", "number_overflow", "body_limit",
        "framing_conflict", "transfer_encoding", "content_encoding", "chunk_syntax",
        "chunk_meta_limit", "trailer_syntax", "trailer_limit", "trailer_framing", "crlf",
        "truncated", "extra_data", "unauthenticated_close", "transport_error" };
    return (unsigned)error < sizeof(names)/sizeof(*names) ? names[error] : "unknown";
}
const char* ra_http_phase_name(int phase)
{
    static const char* const names[] = { "status", "headers", "length_body", "close_body",
        "chunk_size", "chunk_body", "chunk_cr", "chunk_lf", "trailers", "done" };
    return phase == -1 ? "not_started" : (unsigned)phase < sizeof(names)/sizeof(*names) ? names[phase] : "unknown";
}
const char* ra_http_encoding_name(int encoding)
{
    static const char* const names[] = { "absent", "identity", "gzip", "deflate", "br", "other" };
    return (unsigned)encoding < sizeof(names)/sizeof(*names) ? names[encoding] : "unknown";
}
