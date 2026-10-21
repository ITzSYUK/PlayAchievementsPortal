#include "ra_https.h"
#include "ra_http_body.h"
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef union { size_t size; long double align; void* p; uint64_t n; } allocation;
static size_t used, peak;
static const ra_https_options* active;
static void* allocate(size_t n, size_t size)
{
    allocation* p;
    size_t bytes;
    if (size && n > SIZE_MAX / size) return NULL;
    bytes = n * size;
    if (used > RA_HTTPS_HEAP_LIMIT || bytes > RA_HTTPS_HEAP_LIMIT - used ||
        sizeof(*p) > RA_HTTPS_HEAP_LIMIT - used - bytes) return NULL;
    p = calloc(1, sizeof(*p) + bytes); if (!p) return NULL;
    p->size = sizeof(*p) + bytes; used += p->size;
    if (used > peak) peak = used;
    return p + 1;
}
static void release(void* pointer)
{
    allocation* p;
    size_t size;
    if (!pointer) return;
    p = (allocation*)pointer - 1; size = p->size; used -= size;
    mbedtls_platform_zeroize(p, size); free(p);
}
mbedtls_ms_time_t mbedtls_ms_time(void)
{ return active ? (mbedtls_ms_time_t)active->now_ms(active->context) : 0; }
static mbedtls_time_t utc(mbedtls_time_t* out)
{
    mbedtls_time_t now = active ? active->utc(active->context) : (mbedtls_time_t)-1;
    if (out) *out = now;
    return now;
}
static int check(void)
{
    if (active->now_ms(active->context) >= active->deadline_ms) return RA_HTTPS_TIMEOUT;
    if (active->cancelled(active->context)) return RA_HTTPS_CANCEL;
    if (utc(NULL) == (mbedtls_time_t)-1) return RA_HTTPS_INVALID;
    return 0;
}
static int again(int rc)
{
    uint64_t remaining;
    int error = check(); if (error) return error;
    if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) return rc;
    remaining = active->now_ms(active->context);
    if (remaining >= active->deadline_ms) return RA_HTTPS_TIMEOUT;
    remaining = active->deadline_ms - remaining;
    return active->wait(active->context, rc == MBEDTLS_ERR_SSL_WANT_WRITE,
        remaining > 50 ? 50 : (unsigned)remaining);
}
static int http_failure(int error)
{
    switch (error) {
        case RA_HTTP_HEADERS_TOO_LARGE: case RA_HTTP_LINE_TOO_LONG:
        case RA_HTTP_BODY_TOO_LARGE: case RA_HTTP_CHUNK_META_TOO_LARGE:
        case RA_HTTP_TRAILERS_TOO_LARGE: return RA_HTTPS_LIMIT;
        case RA_HTTP_BAD_TRANSFER_ENCODING: case RA_HTTP_BAD_CONTENT_ENCODING: return RA_HTTPS_ENCODING;
        case RA_HTTP_TRUNCATED: case RA_HTTP_UNAUTHENTICATED_CLOSE: return RA_HTTPS_TRUNCATED;
        default: return RA_HTTPS_FRAMING;
    }
}
int ra_https_post(const ra_https_options* o, ra_https_result* r)
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    mbedtls_ctr_drbg_context rng;
    ra_http_body decoder;
    char header[512];
    unsigned char bytes[1024];
    size_t sent, part, length;
    const unsigned char* source;
    int rc, n, state, decoder_ready = 0;
    if (!r) return RA_HTTPS_INVALID;
    memset(r, 0, sizeof(*r)); r->code = RA_HTTPS_INVALID; r->stage = "input"; r->http_phase = -1;
    if (active || used || !o || !o->send || !o->recv || !o->wait || !o->entropy ||
        !o->now_ms || !o->utc || !o->cancelled || !o->ca_der || !o->ca_size || o->ca_size > 8192 ||
        !o->hostname || !o->hostname[0] || strlen(o->hostname) > 128 ||
        strspn(o->hostname, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") != strlen(o->hostname) ||
        !o->post || o->post_size > 8192 || !o->body || !o->capacity || o->capacity > RA_RESPONSE_BODY_LIMIT) return r->code;
    active = o; peak = 0;
    mbedtls_platform_set_calloc_free(allocate, release); mbedtls_platform_set_time(utc);
    mbedtls_ssl_init(&ssl); mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&ca); mbedtls_ctr_drbg_init(&rng);
#define STEP(call) do { rc = check(); if (rc) goto end; rc = (call); if (rc) goto end; } while (0)
    r->stage = "CA_parse";
    STEP(mbedtls_x509_crt_parse_der(&ca, o->ca_der, o->ca_size));
    r->stage = "entropy";
    STEP(mbedtls_ctr_drbg_seed(&rng, o->entropy, o->context, (const unsigned char*)"RAClientTLS/0.11", 16));
    r->stage = "TLS_configuration";
    STEP(mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT));
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &rng);
    STEP(mbedtls_ssl_setup(&ssl, &conf));
    STEP(mbedtls_ssl_set_hostname(&ssl, o->hostname));
    mbedtls_ssl_set_bio(&ssl, o->context, o->send, o->recv, NULL);
    r->stage = "TLS_handshake";
    do {
        rc = check(); if (rc) goto end;
        rc = mbedtls_ssl_handshake(&ssl); if (!rc) break;
        rc = again(rc); if (rc) goto end;
    } while (1);
    r->verify_flags = mbedtls_ssl_get_verify_result(&ssl);
    if (r->verify_flags) { rc = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED; goto end; }
    r->stage = "HTTP_send";
    n = snprintf(header, sizeof(header), "POST /dorequest.php HTTP/1.1\r\nHost: %s\r\nUser-Agent: RAClientPSP/0.11 rcheevos/12.5.0\r\nContent-Type: application/x-www-form-urlencoded\r\nAccept-Encoding: identity\r\nContent-Length: %u\r\nConnection: close\r\n\r\n", o->hostname, (unsigned)o->post_size);
    if (n <= 0 || (size_t)n >= sizeof(header)) { rc = RA_HTTPS_INVALID; goto end; }
    for (part = 0; part < 2; ++part) {
        source = part ? (const unsigned char*)o->post : (const unsigned char*)header;
        length = part ? o->post_size : (size_t)n;
        for (sent = 0; sent < length;) {
            rc = check(); if (rc) goto end;
            /* Fits the fixed 1 KiB outbound TLS record. */
            rc = mbedtls_ssl_write(&ssl, source + sent, length - sent > 1024 ? 1024 : length - sent);
            if (rc > 0) { sent += (size_t)rc; continue; }
            if (!rc) { rc = RA_HTTPS_INVALID; goto end; }
            rc = again(rc); if (rc) goto end;
        }
    }
    ra_http_body_init(&decoder, o->body, o->capacity);
    decoder_ready = 1;
    r->stage = "HTTP_body";
    do {
        rc = check(); if (rc) goto end;
        rc = mbedtls_ssl_read(&ssl, bytes, sizeof(bytes));
        if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) { state = ra_http_body_eof(&decoder); }
        else if (rc > 0) { r->wire_bytes += (size_t)rc; state = ra_http_body_feed(&decoder, bytes, (size_t)rc); }
        else {
            if (!rc || rc == MBEDTLS_ERR_SSL_CONN_EOF) {
                decoder.error = RA_HTTP_UNAUTHENTICATED_CLOSE; rc = RA_HTTPS_TRUNCATED; goto end;
            }
            if (rc == RA_HTTPS_CANCEL || rc == RA_HTTPS_TIMEOUT) goto end;
            if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
                decoder.error = RA_HTTP_TRANSPORT_ERROR; goto end; /* Preserve native TLS/BIO code. */
            }
            rc = again(rc); if (rc) goto end; continue;
        }
        if (state < 0) { rc = http_failure(decoder.error); goto end; }
        if (state == 1) { r->status = decoder.status; r->body_size = decoder.size; r->stage = "complete"; rc = 0; break; }
    } while (1);
end:
    if (decoder_ready) {
        r->status = decoder.status; r->http_error = decoder.error; r->http_phase = decoder.phase;
        r->body_received = decoder.size; r->header_bytes = decoder.header_bytes;
        r->trailer_bytes = decoder.trailer_bytes; r->chunk_meta_bytes = decoder.chunk_meta_bytes;
        r->line_bytes = decoder.line_size;
        r->declared_length = decoder.declared_length; r->has_length = decoder.has_length;
        r->chunked = decoder.chunked; r->content_encoding = decoder.content_encoding;
        r->length_overflow = decoder.length_overflow; r->last_chunk = decoder.last_chunk;
        r->chunk_count = decoder.chunk_count;
        mbedtls_platform_zeroize(&decoder, sizeof(decoder));
    }
    {
        uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
        if (flags != UINT32_MAX) r->verify_flags = flags;
    }
    if (!rc) (void)mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&ca); mbedtls_ctr_drbg_free(&rng);
    r->code = rc; r->heap_peak = peak; r->heap_remaining = used;
    if (rc) { mbedtls_platform_zeroize(o->body, o->capacity); r->body_size = 0; }
    mbedtls_platform_zeroize(bytes, sizeof(bytes));
    mbedtls_platform_set_calloc_free(calloc, free); mbedtls_platform_set_time(time);
    active = NULL; return rc;
#undef STEP
}
