#define _POSIX_C_SOURCE 200809L
#include "tls_probe.h"
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { int fd; } host_io;
static int log_write(const void* p, unsigned n, void* unused)
{ (void)unused; return (int)fwrite(p, 1, n, stdout); }
static uint64_t now_ms(void* unused)
{
    struct timespec t;
    (void)unused; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (unsigned long)t.tv_nsec / 1000000;
}
static time_t utc_time(void* unused)
{
    const char* test_value = getenv("RA_TEST_UTC_EPOCH");
    (void)unused;
    return test_value ? (time_t)strtoll(test_value, NULL, 10) : time(NULL);
}
static int wait_io(void* arg, int writing, unsigned timeout)
{
    struct pollfd p = { ((host_io*)arg)->fd, writing ? POLLOUT : POLLIN, 0 };
    int rc = poll(&p, 1, (int)timeout);
    return rc < 0 && errno != EINTR ? MBEDTLS_ERR_NET_RECV_FAILED : 0;
}
static int bio_send(void* arg, const unsigned char* p, size_t n)
{
    int rc = (int)send(((host_io*)arg)->fd, p, n, MSG_NOSIGNAL);
    if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    return rc < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : rc;
}
static int bio_recv(void* arg, unsigned char* p, size_t n)
{
    int rc = (int)recv(((host_io*)arg)->fd, p, n, 0);
    if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return MBEDTLS_ERR_SSL_WANT_READ;
    return rc < 0 ? MBEDTLS_ERR_NET_RECV_FAILED : rc;
}
static int entropy(void* arg, unsigned char* p, size_t n)
{
    (void)arg;
    if (getenv("RA_TEST_ENTROPY_FAILURE")) return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    while (n) {
        ssize_t rc = getrandom(p, n, 0);
        if (rc < 0 && errno == EINTR) continue;
        if (rc <= 0) return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
        p += rc; n -= (size_t)rc;
    }
    return 0;
}
int main(int argc, char** argv)
{
    struct sockaddr_in address;
    host_io io;
    ra_probe_log log;
    ra_tls_options options;
    ra_tls_result result;
    int rc, error = 0;
    socklen_t error_size = sizeof(error);
    if (argc < 5 || argc > 7) {
        fprintf(stderr, "usage: %s IPv4 port hostname CA-file [timeout-ms [path]]\n", argv[0]); return 2;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &address.sin_addr) != 1) return 2;
    io.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (io.fd < 0 || fcntl(io.fd, F_SETFL, O_NONBLOCK) < 0) return 2;
    rc = connect(io.fd, (struct sockaddr*)&address, sizeof(address));
    if (rc < 0 && errno != EINPROGRESS) { close(io.fd); return 2; }
    wait_io(&io, 1, 3000);
    if (getsockopt(io.fd, SOL_SOCKET, SO_ERROR, &error, &error_size) || error) { close(io.fd); return 2; }
    ra_probe_log_init(&log, log_write, NULL);
    memset(&options, 0, sizeof(options));
    options.send = bio_send; options.recv = bio_recv; options.wait_io = wait_io;
    options.entropy = entropy; options.now_ms = now_ms; options.io = &io;
    options.utc_time = utc_time;
    options.log = &log;
    options.hostname = argv[3]; options.ca_file = argv[4]; options.path = argc == 7 ? argv[6] : "/";
    options.timeout_ms = argc >= 6 ? (unsigned)atoi(argv[5]) : 3000;
    rc = ra_tls_probe(&options, &result);
    close(io.fd);
    printf("probe_code=%d flags=0x%08X http_status=%d remaining=%u\n", rc,
        (unsigned)result.verify_flags, result.http_status, (unsigned)result.memory_remaining);
    return rc == 0 && result.memory_remaining == 0 && log.error == 0 ? 0 : 1;
}
