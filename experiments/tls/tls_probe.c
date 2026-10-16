#include "tls_probe.h"
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define CA_MAX (256u * 1024u)
#define HEADER_MAX 8192u
/* Include alignment suitable for every allocation made by this configuration. */
typedef union { size_t bytes; long double alignment; void* pointer; uint64_t wide; } alloc_header;
static size_t used, peak;
static const ra_tls_options* active;

static void* counted_calloc(size_t n, size_t size)
{
    size_t bytes;
    alloc_header* p;
    if (size && n > SIZE_MAX / size) return NULL;
    bytes = n * size;
    if (bytes > RA_TLS_MEMORY_LIMIT - used || sizeof(*p) > RA_TLS_MEMORY_LIMIT - used - bytes)
        return NULL;
    p = calloc(1, sizeof(*p) + bytes);
    if (!p) return NULL;
    p->bytes = sizeof(*p) + bytes;
    used += p->bytes;
    if (used > peak) peak = used;
    return p + 1;
}
static void counted_free(void* ptr)
{
    alloc_header* p;
    if (!ptr) return;
    p = (alloc_header*)ptr - 1;
    used -= p->bytes;
    free(p);
}
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return active ? (mbedtls_ms_time_t)active->now_ms(active->io) : 0;
}
static mbedtls_time_t utc_time(mbedtls_time_t* output)
{
    mbedtls_time_t value = active ? active->utc_time(active->io) : (mbedtls_time_t)-1;
    if (output) *output = value;
    return value;
}

static int wait_again(int code, uint64_t deadline)
{
    uint64_t now;
    unsigned wait;
    if (code != MBEDTLS_ERR_SSL_WANT_READ && code != MBEDTLS_ERR_SSL_WANT_WRITE)
        return code;
    now = active->now_ms(active->io);
    if (now >= deadline) return RA_TLS_TIMEOUT;
    wait = deadline - now > 100 ? 100 : (unsigned)(deadline - now);
    code = active->wait_io(active->io, code == MBEDTLS_ERR_SSL_WANT_WRITE, wait);
    return code < 0 ? code : 0;
}

static int load_ca(mbedtls_x509_crt* ca, const char* path)
{
    FILE* file = fopen(path, "rb");
    unsigned char* data = NULL;
    long size;
    int result = RA_TLS_CA_LIMIT;
    if (!file) return result;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 || size > (long)CA_MAX ||
        fseek(file, 0, SEEK_SET)) goto end;
    data = counted_calloc((size_t)size + 1, 1);
    if (!data) goto end;
    if (fread(data, 1, (size_t)size, file) != (size_t)size || ferror(file)) goto end;
    /* A positive parse result means that some certificates failed: reject it. */
    result = mbedtls_x509_crt_parse(ca, data, (size_t)size + 1);
    if (result > 0) result = MBEDTLS_ERR_X509_INVALID_FORMAT;
end:
    counted_free(data);
    if (fclose(file) && result == 0) result = RA_TLS_CA_LIMIT;
    return result;
}

static void log_error(ra_tls_result* r)
{
    char message[256];
    char flags[2048];
    char* line;
    if (r->code == RA_TLS_TIMEOUT) strcpy(message, "operation deadline exceeded");
    else if (r->code == RA_TLS_HEADER_LIMIT) strcpy(message, "HTTP headers exceed 8192 bytes");
    else if (r->code == RA_TLS_BAD_HTTP) strcpy(message, "invalid or incomplete HTTP headers/status");
    else if (r->code == RA_TLS_CA_LIMIT) strcpy(message, "CA file unavailable, incomplete or over 256 KiB");
    else if (r->code == RA_TLS_CLOCK_ERROR) strcpy(message, "UTC clock unavailable");
    else mbedtls_strerror(r->code, message, sizeof(message));
    ra_probe_log_printf(active->log, "tls_stage=%s code=%d (-0x%04X) %s\n",
        r->stage, r->code, r->code < 0 ? (unsigned)(-r->code) : 0u, message);
    ra_probe_log_printf(active->log, "verify_flags=0x%08X\n", (unsigned)r->verify_flags);
    if (!r->verify_flags) return;
    mbedtls_x509_crt_verify_info(flags, sizeof(flags), "", r->verify_flags);
    line = flags;
    while (*line) {
        char* end = strchr(line, '\n');
        if (end) *end = 0;
        ra_probe_log_printf(active->log, "certificate: %s\n", line);
        if (!end) break;
        line = end + 1;
    }
}

int ra_tls_probe(const ra_tls_options* o, ra_tls_result* r)
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    mbedtls_ctr_drbg_context rng;
    char request[512];
    unsigned char headers[HEADER_MAX + 1];
    size_t bytes = 0, sent = 0;
    int rc, request_length;
    uint64_t deadline;
    memset(r, 0, sizeof(*r));
    r->stage = "configuration";
    r->protocol = r->cipher = "none";
    if (active || used || !o || !o->send || !o->recv || !o->wait_io || !o->entropy ||
        !o->now_ms || !o->utc_time || !o->hostname || !o->path || !o->ca_file || !o->timeout_ms ||
        o->path[0] != '/' || strpbrk(o->hostname, "\r\n") || strpbrk(o->path, "\r\n"))
        return r->code = RA_TLS_BAD_HTTP;
    active = o;
    peak = 0;
    mbedtls_platform_set_calloc_free(counted_calloc, counted_free);
    mbedtls_platform_set_time(utc_time);
    mbedtls_ssl_init(&ssl); mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&ca); mbedtls_ctr_drbg_init(&rng);
#define STEP(stage_name, expression) do { r->stage = stage_name; \
    ra_probe_log_printf(o->log, "begin: %s\n", r->stage); \
    rc = (expression); if (rc != 0) goto cleanup; } while (0)
    STEP("UTC clock", utc_time(NULL) == (mbedtls_time_t)-1 ? RA_TLS_CLOCK_ERROR : 0);
    ra_probe_log_printf(o->log, "tls_utc_epoch=%lld\n", (long long)utc_time(NULL));
    STEP("CA parse", load_ca(&ca, o->ca_file));
    STEP("strong entropy / CTR_DRBG seed", mbedtls_ctr_drbg_seed(&rng, o->entropy, o->random,
        (const unsigned char*)"RA-PSP-MbedTLS-Probe/0.2", 23));
    STEP("TLS configuration", mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT));
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &rng);
    STEP("TLS setup", mbedtls_ssl_setup(&ssl, &conf));
    STEP("hostname and SNI", mbedtls_ssl_set_hostname(&ssl, o->hostname));
    mbedtls_ssl_set_bio(&ssl, o->io, o->send, o->recv, NULL);
    deadline = o->now_ms(o->io) + o->timeout_ms;
    r->stage = "TLS handshake";
    ra_probe_log_printf(o->log, "begin: TLS handshake\n");
    do {
        if (o->now_ms(o->io) >= deadline) { rc = RA_TLS_TIMEOUT; goto cleanup; }
        rc = mbedtls_ssl_handshake(&ssl);
        if (rc == 0) break;
        rc = wait_again(rc, deadline);
        if (rc) goto cleanup;
    } while (1);
    r->verify_flags = mbedtls_ssl_get_verify_result(&ssl);
    if (r->verify_flags) { rc = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED; goto cleanup; }
    r->protocol = mbedtls_ssl_get_version(&ssl);
    r->cipher = mbedtls_ssl_get_ciphersuite(&ssl);
    ra_probe_log_printf(o->log, "certificate_verified=1 protocol=%s cipher=%s\n", r->protocol, r->cipher);
    request_length = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: RA-PSP-MbedTLS-Probe/0.2\r\nConnection: close\r\n\r\n",
        o->path, o->hostname);
    if (request_length <= 0 || (size_t)request_length >= sizeof(request)) { rc = RA_TLS_BAD_HTTP; goto cleanup; }
    r->stage = "HTTP send";
    deadline = o->now_ms(o->io) + o->timeout_ms;
    while (sent < (size_t)request_length) {
        if (o->now_ms(o->io) >= deadline) { rc = RA_TLS_TIMEOUT; goto cleanup; }
        rc = mbedtls_ssl_write(&ssl, (unsigned char*)request + sent, (size_t)request_length - sent);
        if (rc > 0) { sent += (size_t)rc; continue; }
        if (rc == 0) { rc = RA_TLS_BAD_HTTP; goto cleanup; }
        rc = wait_again(rc, deadline);
        if (rc) goto cleanup;
    }
    r->stage = "HTTP headers";
    deadline = o->now_ms(o->io) + o->timeout_ms;
    while (1) {
        char* end;
        if (bytes == HEADER_MAX) { rc = RA_TLS_HEADER_LIMIT; goto cleanup; }
        if (o->now_ms(o->io) >= deadline) { rc = RA_TLS_TIMEOUT; goto cleanup; }
        rc = mbedtls_ssl_read(&ssl, headers + bytes, HEADER_MAX - bytes);
        if (rc <= 0) {
            if (rc == 0 || rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) { rc = RA_TLS_BAD_HTTP; goto cleanup; }
            rc = wait_again(rc, deadline);
            if (rc) goto cleanup;
            continue;
        }
        /* Embedded NUL is not valid in an HTTP header. Body bytes are ignored. */
        bytes += (size_t)rc;
        headers[bytes] = 0;
        end = strstr((char*)headers, "\r\n\r\n");
        if (!end) {
            if (memchr(headers, 0, bytes)) { rc = RA_TLS_BAD_HTTP; goto cleanup; }
            continue;
        }
        if (strncmp((char*)headers, "HTTP/1.1 ", 9) && strncmp((char*)headers, "HTTP/1.0 ", 9)) {
            rc = RA_TLS_BAD_HTTP; goto cleanup;
        }
        if (end - (char*)headers < 12 || headers[9] < '1' || headers[9] > '5' ||
            headers[10] < '0' || headers[10] > '9' || headers[11] < '0' || headers[11] > '9' ||
            (headers[12] != ' ' && headers[12] != '\r')) { rc = RA_TLS_BAD_HTTP; goto cleanup; }
        r->http_status = (headers[9] - '0') * 100 + (headers[10] - '0') * 10 + headers[11] - '0';
        r->sample_bytes = (unsigned)bytes;
        ra_probe_log_printf(o->log, "http_status=%d sample_bytes=%u (body not fully read)\n", r->http_status, r->sample_bytes);
        rc = 0; r->stage = "TLS and HTTP headers verified";
        break;
    }
cleanup:
    /* On handshake failure this preserves the concrete X.509 flags. */
    {
        uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
        /* UINT32_MAX means verification never took place, not every error. */
        if (flags != UINT32_MAX) r->verify_flags = flags;
    }
    r->code = rc;
    log_error(r);
    /* One nonblocking close-notify attempt; TCP close belongs to the caller. */
    if (rc == 0) (void)mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&rng); mbedtls_x509_crt_free(&ca);
    r->memory_peak = peak; r->memory_remaining = used;
    ra_probe_log_printf(o->log, "tls_heap_peak_bytes=%u tls_heap_remaining_bytes=%u limit_bytes=%u\n",
        (unsigned)peak, (unsigned)used, RA_TLS_MEMORY_LIMIT);
    mbedtls_platform_set_calloc_free(calloc, free);
    mbedtls_platform_set_time(time);
    active = NULL;
    return r->code;
#undef STEP
}
