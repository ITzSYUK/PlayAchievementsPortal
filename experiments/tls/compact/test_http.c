#include "ra_http_body.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void test(const char* text, int success, const char* expected)
{
    unsigned char body[16];
    ra_http_body d;
    size_t i;
    int rc = 0;
    ra_http_body_init(&d, body, sizeof(body));
    for (i = 0; text[i] && rc >= 0; ++i) rc = ra_http_body_feed(&d, (const unsigned char*)text + i, 1);
    if (!rc) rc = ra_http_body_eof(&d);
    assert((rc == 1) == success);
    if (success) { assert(d.status == 200 && d.size == strlen(expected)); assert(!memcmp(body, expected, d.size)); }
}
static void error(const char* text, int expected, int phase, size_t received)
{
    unsigned char body[32];
    ra_http_body d;
    size_t i; int rc = 0;
    ra_http_body_init(&d, body, sizeof(body));
    for (i = 0; text[i] && rc >= 0; ++i) rc = ra_http_body_feed(&d, (const unsigned char*)text + i, 1);
    if (!rc) rc = ra_http_body_eof(&d);
    assert(rc == -1 && d.error == expected && d.phase == phase && d.size == received);
    assert(d.status == 200);
    assert(ra_http_body_feed(&d, NULL, 1) == -1 && d.error == expected);
    assert(ra_http_body_eof(&d) == -1 && d.error == expected);
}
static void limits(void)
{
    static unsigned char body[RA_RESPONSE_BODY_LIMIT];
    unsigned char bytes[1024];
    ra_http_body d;
    unsigned i; int rc;
    char length[80], over[80], over_chunk[24];
    const char chunked[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    memset(bytes, 'x', sizeof(bytes));
    snprintf(length, sizeof(length), "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n\r\n", RA_RESPONSE_BODY_LIMIT);
    snprintf(over, sizeof(over), "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n\r\n", RA_RESPONSE_BODY_LIMIT + 1);
    snprintf(over_chunk, sizeof(over_chunk), "%X\r\n", RA_RESPONSE_BODY_LIMIT + 1);
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)over, strlen(over)) == -1);
    assert(d.status == 200 && d.error == RA_HTTP_BODY_TOO_LARGE && d.declared_length == RA_RESPONSE_BODY_LIMIT + 1 && d.size == 0);
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)length, strlen(length)) == 0);
    for (i = 0; i < RA_RESPONSE_BODY_LIMIT / sizeof(bytes); ++i) {
        rc = ra_http_body_feed(&d, bytes, sizeof(bytes)); assert(rc == (i + 1 == RA_RESPONSE_BODY_LIMIT / sizeof(bytes)));
    }
    assert(d.size == sizeof(body) && !memcmp(body, bytes, sizeof(bytes)) && ra_http_body_eof(&d) == 1);
    assert(ra_http_body_feed(&d, bytes, 1) == -1 && d.error == RA_HTTP_EXTRA_DATA);
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)"HTTP/1.0 200 OK\r\n\r\n", 19) == 0);
    for (i = 0; i < RA_RESPONSE_BODY_LIMIT / sizeof(bytes); ++i) assert(ra_http_body_feed(&d, bytes, sizeof(bytes)) == 0);
    assert(ra_http_body_feed(&d, bytes, 1) == -1 && d.error == RA_HTTP_BODY_TOO_LARGE && d.size == sizeof(body));
    /* Many legal small chunks used to spend the 4096-byte HEADER budget. */
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)chunked, sizeof(chunked)-1) == 0);
    for (i = 0; i < 1500; ++i)
        assert(ra_http_body_feed(&d, (const unsigned char*)"1\r\nx\r\n", 6) == 0);
    assert(ra_http_body_feed(&d, (const unsigned char*)"0\r\n\r\n", 5) == 1);
    assert(d.header_bytes == sizeof(chunked)-1 && d.size == 1500 && d.chunk_count == 1500 && d.chunk_meta_bytes > 4096);
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)chunked, sizeof(chunked)-1) == 0);
    for (i = 0; i < 4000; ++i) if (ra_http_body_feed(&d, (const unsigned char*)"1\r\nx\r\n", 6) < 0) break;
    assert(i < 4000 && d.error == RA_HTTP_CHUNK_META_TOO_LARGE && d.chunk_meta_bytes == RA_HTTP_CHUNK_META_LIMIT + 1);
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)chunked, sizeof(chunked)-1) == 0);
    assert(ra_http_body_feed(&d, (const unsigned char*)over_chunk, strlen(over_chunk)) == -1);
    assert(d.error == RA_HTTP_BODY_TOO_LARGE && d.last_chunk == RA_RESPONSE_BODY_LIMIT + 1 && !d.size);
}
static void large_chunks(void)
{
    static unsigned char body[RA_RESPONSE_BODY_LIMIT];
    unsigned char bytes[997];
    const unsigned sizes[] = { 129891, RA_RESPONSE_BODY_LIMIT };
    const char header[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    ra_http_body d;
    unsigned scenario;
    memset(bytes, 'x', sizeof(bytes));
    for (scenario = 0; scenario < sizeof(sizes)/sizeof(*sizes); ++scenario) {
        char chunk[24]; size_t remaining = sizes[scenario];
        snprintf(chunk, sizeof(chunk), "%X\r\n", sizes[scenario]);
        ra_http_body_init(&d, body, sizeof(body));
        assert(ra_http_body_feed(&d, (const unsigned char*)header, sizeof(header)-1) == 0);
        assert(ra_http_body_feed(&d, (const unsigned char*)chunk, strlen(chunk)) == 0 && d.last_chunk == sizes[scenario]);
        while (remaining) {
            size_t part = remaining > sizeof(bytes) ? sizeof(bytes) : remaining;
            assert(ra_http_body_feed(&d, bytes, part) == 0); remaining -= part;
        }
        assert(ra_http_body_feed(&d, (const unsigned char*)"\r\n0\r\n\r\n", 7) == 1);
        assert(d.size == sizes[scenario] && d.chunk_count == 1 && d.error == RA_HTTP_OK);
        for (remaining = 0; remaining < d.size; ++remaining) assert(body[remaining] == 'x');
    }
    /* Aggregate size, not just the size of any individual chunk, is capped. */
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)header, sizeof(header)-1) == 0);
    {
        char chunk[24]; size_t remaining = RA_RESPONSE_BODY_LIMIT - 10;
        snprintf(chunk, sizeof(chunk), "%X\r\n", RA_RESPONSE_BODY_LIMIT - 10);
        assert(ra_http_body_feed(&d, (const unsigned char*)chunk, strlen(chunk)) == 0);
        while (remaining) {
            size_t part = remaining > sizeof(bytes) ? sizeof(bytes) : remaining;
            assert(ra_http_body_feed(&d, bytes, part) == 0); remaining -= part;
        }
    }
    assert(ra_http_body_feed(&d, (const unsigned char*)"\r\n14\r\n", 6) == -1);
    assert(d.error == RA_HTTP_BODY_TOO_LARGE && d.size == RA_RESPONSE_BODY_LIMIT - 10 && d.last_chunk == 20);
}
static void metadata_limits(void)
{
    unsigned char body[4];
    char field[1028];
    ra_http_body d;
    unsigned i;
    const char start[] = "HTTP/1.1 200 OK\r\n";
    memset(field, 'x', sizeof(field)); field[0] = 'X'; field[1] = ':';
    field[1022] = '\r'; field[1023] = '\n';
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)start, sizeof(start)-1) == 0);
    assert(ra_http_body_feed(&d, (const unsigned char*)field, 1024) == 0);
    assert(ra_http_body_feed(&d, (const unsigned char*)"\r\n", 2) == 0 && ra_http_body_eof(&d) == 1);
    field[1022] = 'x'; field[1023] = '\r'; field[1024] = '\n';
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)start, sizeof(start)-1) == 0);
    assert(ra_http_body_feed(&d, (const unsigned char*)field, 1025) == -1 && d.error == RA_HTTP_LINE_TOO_LONG);
    field[898] = '\r'; field[899] = '\n';
    ra_http_body_init(&d, body, sizeof(body));
    assert(ra_http_body_feed(&d, (const unsigned char*)start, sizeof(start)-1) == 0);
    for (i = 0; i < 5; ++i) if (ra_http_body_feed(&d, (const unsigned char*)field, 900) < 0) break;
    assert(i < 5 && d.error == RA_HTTP_HEADERS_TOO_LARGE && d.header_bytes == RA_HTTP_HEADER_LIMIT + 1);
    ra_http_body_init(&d, body, sizeof(body));
    {
        const char trailers[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n";
        assert(ra_http_body_feed(&d, (const unsigned char*)trailers, sizeof(trailers)-1) == 0);
    }
    for (i = 0; i < 3; ++i) if (ra_http_body_feed(&d, (const unsigned char*)field, 900) < 0) break;
    assert(i < 3 && d.error == RA_HTTP_TRAILERS_TOO_LARGE && d.trailer_bytes == RA_HTTP_TRAILER_LIMIT + 1);
}
int main(void)
{
    test("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK", 1, "OK");
    test("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nO\r\n1\r\nK\r\n0\r\n\r\n", 1, "OK");
    test("HTTP/1.0 200 OK\r\n\r\nOK", 1, "OK");
    test("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", 1, "");
    test("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nO", 0, "");
    test("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOKextra", 0, "");
    test("HTTP/1.1 200 OK\r\nContent-Length: 9999999999999999999\r\n\r\n", 0, "");
    test("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\nOK", 0, "");
    test("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n", 0, "");
    test("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n11\r\n", 0, "");
    test("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nO!\n0\r\n\r\n", 0, "");
    test("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n", 0, "");
    test("HTTP/1.1 200 OK\n\n", 0, "");
    test("HTTP/1.1 200 OK\r\nX_Test!#$%&'*+-.^_`|~: ignored-secret\r\nContent-Length: 2\r\n\r\nOK", 1, "OK");
    test("HTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked\r\n\r\n0000000000000001 ; foo = \"a;\\\"b\" ;bar\r\nO\r\n1;id=two\r\nK\r\n0;end\r\nX-Result: ignored-secret\r\n\r\n", 1, "OK");
    error("HTTP/1.1 200 OK\r\nContent-Length: 40\r\n\r\n", RA_HTTP_BODY_TOO_LARGE, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nContent-Length: 4294967296\r\n\r\n", RA_HTTP_NUMBER_OVERFLOW, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\n", RA_HTTP_BAD_LENGTH, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nO", RA_HTTP_TRUNCATED, RA_HTTP_LENGTH, 1);
    error("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOKx", RA_HTTP_EXTRA_DATA, RA_HTTP_DONE, 2);
    error("HTTP/1.1 200 OK\r\nX Bad: value\r\n", RA_HTTP_BAD_HEADER, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n", RA_HTTP_BAD_CONTENT_ENCODING, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n", RA_HTTP_BAD_TRANSFER_ENCODING, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n", RA_HTTP_AMBIGUOUS, RA_HTTP_HEADER, 0);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1;=bad\r\n", RA_HTTP_BAD_CHUNK, RA_HTTP_CHUNK_SIZE, 0);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1;foo=\"bad\r\n", RA_HTTP_BAD_CHUNK, RA_HTTP_CHUNK_SIZE, 0);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n100000000\r\n", RA_HTTP_NUMBER_OVERFLOW, RA_HTTP_CHUNK_SIZE, 0);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nO!\n", RA_HTTP_BAD_CRLF, RA_HTTP_CHUNK_CR, 1);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 0\r\n", RA_HTTP_TRAILER_FRAMING, RA_HTTP_TRAILER, 0);
    error("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nnot a field\r\n", RA_HTTP_BAD_TRAILER, RA_HTTP_TRAILER, 0);
    limits(); large_chunks(); metadata_limits();
    assert(!strcmp(ra_http_error_name(RA_HTTP_BODY_TOO_LARGE), "body_limit"));
    assert(!strcmp(ra_http_error_name(-1), "unknown") && !strcmp(ra_http_phase_name(-1), "not_started"));
    puts("PASS HTTP framing: observed 129891-byte chunk, exact 256KiB and aggregate limits, retained diagnostics, fragmented extensions/trailers/tokens, metadata budgets");
    return 0;
}
