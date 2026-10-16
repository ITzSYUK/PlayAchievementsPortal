#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psprtc.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/platform_util.h>
#include "entropy_request.h"
#include "tls_probe.h"
#include "rtc_epoch.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

PSP_MODULE_INFO("RAMbedTLSProbe", 0, 0, 2);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);
SceUID kuKernelLoadModule(const char*, int, SceKernelLMOption*);

static const char* const hosts[] = {
    "retroachievements.org", "sha256.badssl.com", "expired.badssl.com",
    "wrong.host.badssl.com", "self-signed.badssl.com", "ra-probe-no-such-host.invalid"
};
static const int modules[] = { PSP_NET_MODULE_COMMON, PSP_NET_MODULE_INET };
static int profile = 1;
static unsigned endpoint = 1;
static volatile int done;
static int result_code, first_failure, socket_fd = -1;
static int cleanup_failure;
static const char* failure_stage = "none";
static const char* outcome = "FAILURE";
static ra_tls_result tls_result;
static ra_probe_log log_data;
static SceUID log_fd = -1;
static char app_dir[512], log_path[512], ca_path[512], entropy_path[512];
static ra_entropy_output seed __attribute__((aligned(64)));
static size_t seed_offset;
static u64 rtc_epoch_tick;
static u32 rtc_resolution;
static int rtc_error;

static int log_write(const void* data, unsigned n, void* unused)
{ (void)unused; return sceIoWrite(log_fd, data, n); }
static int record(const char* stage, int rc)
{
    ra_probe_log_printf(&log_data, "%s: 0x%08X\n", stage, (unsigned)rc);
    if (rc < 0 && !first_failure) { first_failure = rc; failure_stage = stage; }
    return rc;
}
static void record_cleanup(const char* stage, int rc)
{
    record(stage, rc);
    if (rc < 0 && !cleanup_failure) cleanup_failure = rc;
}
static uint64_t now_ms(void* unused)
{ (void)unused; return sceKernelGetSystemTimeWide() / 1000; }
static time_t rtc_utc_time(void* unused)
{
    u64 current;
    int64_t seconds;
    time_t result;
    int rc;
    (void)unused;
    rc = sceRtcGetCurrentTick(&current);
    if (rc < 0) { rtc_error = rc; return (time_t)-1; }
    if (ra_rtc_epoch_seconds(current, rtc_epoch_tick, rtc_resolution, &seconds)) {
        rtc_error = RA_TLS_CLOCK_ERROR; return (time_t)-1;
    }
    result = (time_t)seconds;
    if ((int64_t)result != seconds) { rtc_error = RA_TLS_CLOCK_ERROR; return (time_t)-1; }
    return result;
}
static int wait_io(void* unused, int writing, unsigned timeout)
{
    SceNetInetPollfd fd = { socket_fd, writing ? SCE_NET_INET_POLLOUT : SCE_NET_INET_POLLIN, 0 };
    int rc;
    (void)unused;
    rc = sceNetInetPoll(&fd, 1, (int)timeout);
    if (rc < 0) {
        int error = sceNetInetGetErrno();
        if (error == EINTR) return 0;
        record("poll network errno", error);
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    return 0;
}
static int retryable(int error)
{ return error == EAGAIN || error == EWOULDBLOCK || error == EINTR; }
static int bio_send(void* unused, const unsigned char* p, size_t n)
{
    int rc, error;
    (void)unused;
    rc = (int)sceNetInetSend(socket_fd, p, n, 0);
    if (rc >= 0) return rc;
    error = sceNetInetGetErrno();
    if (retryable(error)) return MBEDTLS_ERR_SSL_WANT_WRITE;
    record("send network errno", error);
    return MBEDTLS_ERR_NET_SEND_FAILED;
}
static int bio_recv(void* unused, unsigned char* p, size_t n)
{
    int rc, error;
    (void)unused;
    rc = (int)sceNetInetRecv(socket_fd, p, n, 0);
    if (rc >= 0) return rc;
    error = sceNetInetGetErrno();
    if (retryable(error)) return MBEDTLS_ERR_SSL_WANT_READ;
    record("receive network errno", error);
    return MBEDTLS_ERR_NET_RECV_FAILED;
}
static int entropy_read(void* unused, unsigned char* data, size_t n)
{
    (void)unused;
    if (seed.status != 0 || n > sizeof(seed.bytes) - seed_offset)
        return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    memcpy(data, seed.bytes + seed_offset, n);
    mbedtls_platform_zeroize(seed.bytes + seed_offset, n);
    seed_offset += n;
    return 0;
}
static int obtain_entropy(void)
{
    ra_entropy_request request = { &seed };
    SceUID module;
    int rc, status = -1;
    memset(&seed, 0, sizeof(seed)); seed.status = -1; seed_offset = 0;
    ra_probe_log_printf(&log_data, "begin: load KIRK entropy helper\n");
    module = record("load KIRK helper", kuKernelLoadModule(entropy_path, 0, NULL));
    if (module < 0) return module;
    sceKernelDcacheWritebackInvalidateRange(&seed, sizeof(seed));
    rc = record("start KIRK helper", sceKernelStartModule(module, sizeof(request), &request, &status, NULL));
    sceKernelDcacheInvalidateRange(&seed, sizeof(seed));
    record("KIRK helper start status", status);
    if (rc >= 0 && status == 0) {
        int stop_status = -1;
        rc = seed.status;
        record("KIRK command 14 result", rc);
        /* Stop/unload failures also prevent this probe proceeding. */
        if (record("stop KIRK helper", sceKernelStopModule(module, 0, NULL, &stop_status, NULL)) < 0 ||
            record("KIRK helper stop status", stop_status) < 0) rc = -1;
    } else if (rc >= 0) rc = status < 0 ? status : -1;
    if (record("unload KIRK helper", sceKernelUnloadModule(module)) < 0) rc = -1;
    if (rc == 0) ra_probe_log_printf(&log_data, "entropy_source=PSP KIRK command 14 bytes=%u (not logged)\n", (unsigned)sizeof(seed.bytes));
    /* KIRK can use positive error codes; PSP worker STEP treats positive UIDs
     * as success, so normalize entropy failure before returning to it. */
    if (rc > 0) rc = MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    return rc;
}
static int exit_callback(int a, int b, void* p)
{ (void)a; (void)b; (void)p; sceKernelExitGame(); return 0; }
static int callback_thread(SceSize n, void* p)
{
    int callback = sceKernelCreateCallback("Exit", exit_callback, NULL);
    (void)n; (void)p; sceKernelRegisterExitCallback(callback); sceKernelSleepThreadCB(); return 0;
}
static int worker(SceSize n, void* p)
{
    unsigned loaded = 0;
    int net = 0, inet = 0, resolver = 0, apctl = 0, connected = 0, rid = -1;
    int rc = -1, state, previous_state = -1;
    int nonblock = 1, socket_error = 0;
    socklen_t error_length = sizeof(socket_error);
    uint64_t started;
    struct sockaddr_in address;
    const ScePspDateTime epoch_date = { 1970, 1, 1, 0, 0, 0, 0 };
    ScePspDateTime utc_date;
    time_t utc_epoch;
    char dns_buffer[1024];
    ra_tls_options options;
    (void)n; (void)p;
#define STEP(label, expression) do { ra_probe_log_printf(&log_data, "begin: %s\n", label); \
    rc = record(label, (expression)); if (rc < 0) goto cleanup; } while (0)
    ra_probe_log_printf(&log_data, "probe_version=0.2 backend=MbedTLS-3.6.7 TLS1.2\nprofile_index=%d endpoint_index=%u\n", profile, endpoint);
    ra_probe_log_printf(&log_data, "host=%s libc_utc_epoch=%lld (diagnostic only)\n", hosts[endpoint], (long long)time(NULL));
    STEP("RTC Unix epoch anchor", sceRtcGetTick(&epoch_date, &rtc_epoch_tick));
    rtc_resolution = sceRtcGetTickResolution();
    if (!rtc_resolution) { rc = record("RTC resolution unavailable", RA_TLS_CLOCK_ERROR); goto cleanup; }
    utc_epoch = rtc_utc_time(NULL);
    STEP("RTC UTC clock", utc_epoch == (time_t)-1 ? rtc_error : 0);
    STEP("RTC UTC calendar", sceRtcGetCurrentClock(&utc_date, 0));
    ra_probe_log_printf(&log_data, "rtc_utc_epoch=%lld rtc_tick_resolution=%u\n", (long long)utc_epoch, (unsigned)rtc_resolution);
    ra_probe_log_printf(&log_data, "rtc_utc=%04u-%02u-%02uT%02u:%02u:%02uZ\n", (unsigned)utc_date.year,
        (unsigned)utc_date.month, (unsigned)utc_date.day, (unsigned)utc_date.hour, (unsigned)utc_date.minute, (unsigned)utc_date.second);
    STEP("strong entropy helper", obtain_entropy());
    for (loaded = 0; loaded < sizeof(modules) / sizeof(modules[0]); ++loaded)
        STEP("load net module", sceUtilityLoadNetModule(modules[loaded]));
    STEP("net init", sceNetInit(0x20000, 0x2A, 0, 0x2A, 0)); net = 1;
    STEP("inet init", sceNetInetInit()); inet = 1;
    STEP("resolver init", sceNetResolverInit()); resolver = 1;
    STEP("apctl init", sceNetApctlInit(0x1800, 0x30)); apctl = 1;
    STEP("connect profile", sceNetApctlConnect(profile)); connected = 1;
    started = now_ms(NULL);
    do {
        STEP("get apctl state", sceNetApctlGetState(&state));
        if (state != previous_state) {
            ra_probe_log_printf(&log_data, "apctl_state=%d elapsed_ms=%u\n", state, (unsigned)(now_ms(NULL) - started));
            previous_state = state;
        }
        if (state == PSP_NET_APCTL_STATE_GOT_IP) break;
        if (now_ms(NULL) - started >= 30000) { rc = record("Wi-Fi timeout", RA_TLS_TIMEOUT); goto cleanup; }
        sceKernelDelayThread(250000);
    } while (1);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_port = htons(443);
    STEP("create resolver", sceNetResolverCreate(&rid, dns_buffer, sizeof(dns_buffer)));
    STEP("DNS (7s, one attempt)", sceNetResolverStartNtoA(rid, hosts[endpoint], &address.sin_addr, 7, 1));
    ra_probe_log_printf(&log_data, "resolved_ipv4=%s\n", inet_ntoa(address.sin_addr));
    STEP("TCP socket", sceNetInetSocket(AF_INET, SOCK_STREAM, 0)); socket_fd = rc;
    STEP("nonblocking socket", sceNetInetSetsockopt(socket_fd, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock)));
    ra_probe_log_printf(&log_data, "begin: TCP connect\n");
    rc = sceNetInetConnect(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (rc < 0) {
        int error = sceNetInetGetErrno();
        record("connect network errno", error);
        if (error != EINPROGRESS && error != EALREADY && !retryable(error)) {
            rc = record("TCP connect", MBEDTLS_ERR_NET_CONNECT_FAILED); goto cleanup;
        }
        started = now_ms(NULL);
        while (1) {
            SceNetInetPollfd fd = { socket_fd, SCE_NET_INET_POLLOUT, 0 };
            if (now_ms(NULL) - started >= 7000) { rc = record("TCP timeout", RA_TLS_TIMEOUT); goto cleanup; }
            rc = sceNetInetPoll(&fd, 1, 100);
            if (rc < 0) { record("connect poll errno", sceNetInetGetErrno()); rc = record("TCP poll", rc); goto cleanup; }
            if (rc > 0) break;
        }
        STEP("TCP SO_ERROR", sceNetInetGetsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length));
        record("TCP SO_ERROR value", socket_error);
        if (socket_error) { rc = record("TCP connect failed", MBEDTLS_ERR_NET_CONNECT_FAILED); goto cleanup; }
    }
    memset(&options, 0, sizeof(options));
    options.send = bio_send; options.recv = bio_recv; options.wait_io = wait_io;
    options.entropy = entropy_read; options.now_ms = now_ms;
    options.utc_time = rtc_utc_time;
    options.hostname = hosts[endpoint]; options.path = endpoint == 0 ? "/dorequest.php" : "/";
    options.ca_file = ca_path; options.timeout_ms = 45000; options.log = &log_data;
    started = now_ms(NULL);
    ra_probe_log_printf(&log_data, "kernel_free_before_tls=%u\n", (unsigned)sceKernelTotalFreeMemSize());
    rc = ra_tls_probe(&options, &tls_result);
    record(tls_result.stage, rc);
    ra_probe_log_printf(&log_data, "kernel_free_after_tls=%u elapsed_ms=%u\n", (unsigned)sceKernelTotalFreeMemSize(), (unsigned)(now_ms(NULL) - started));
    if (endpoint < 2 && rc == 0) {
        if (tls_result.http_status >= 200 && tls_result.http_status < 300) outcome = "SUCCESS";
        else { outcome = "TLS_OK_HTTP_ERROR"; rc = record("HTTP status outside 2xx", RA_TLS_BAD_HTTP); }
    } else if (endpoint >= 2 && endpoint <= 4) {
        uint32_t expected = endpoint == 2 ? MBEDTLS_X509_BADCERT_EXPIRED :
            endpoint == 3 ? MBEDTLS_X509_BADCERT_CN_MISMATCH : MBEDTLS_X509_BADCERT_NOT_TRUSTED;
        if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && (tls_result.verify_flags & expected)) {
            outcome = "EXPECTED_CERT_REJECTION"; rc = 0;
        } else if (rc == 0) { outcome = "UNEXPECTED_CERT_ACCEPTED"; rc = record("invalid certificate accepted", -1); }
    }
cleanup:
    if (rtc_error) record("RTC error", rtc_error);
    mbedtls_platform_zeroize(&seed, sizeof(seed));
    if (socket_fd >= 0) { record_cleanup("close TCP", sceNetInetClose(socket_fd)); socket_fd = -1; }
    if (rid >= 0) record_cleanup("delete resolver", sceNetResolverDelete(rid));
    if (connected) record_cleanup("apctl disconnect", sceNetApctlDisconnect());
    if (apctl) record_cleanup("apctl term", sceNetApctlTerm());
    if (resolver) record_cleanup("resolver term", sceNetResolverTerm());
    if (inet) record_cleanup("inet term", sceNetInetTerm());
    if (net) record_cleanup("net term", sceNetTerm());
    while (loaded) { --loaded; record_cleanup("unload net module", sceUtilityUnloadNetModule(modules[loaded])); }
    if (cleanup_failure) { outcome = "FAILURE"; if (rc == 0) rc = cleanup_failure; }
    if (tls_result.memory_remaining) { outcome = "FAILURE"; rc = record("TLS memory still allocated", -1); }
    ra_probe_log_printf(&log_data, "probe_outcome=%s result=0x%08X first_stage=%s first_code=0x%08X\n",
        outcome, (unsigned)rc, failure_stage, (unsigned)first_failure);
    result_code = rc; done = 1;
    return 0;
#undef STEP
}

static int init_paths(int argc, char** argv)
{
    const char* slash = argc && argv && argv[0] ? strrchr(argv[0], '/') : NULL;
    if (slash) {
        size_t n = (size_t)(slash - argv[0]) + 1;
        if (n >= sizeof(app_dir)) return -1;
        memcpy(app_dir, argv[0], n); app_dir[n] = 0;
    } else strcpy(app_dir, "ms0:/PSP/GAME/RAMbedTLSProbe/");
    if (snprintf(log_path, sizeof(log_path), "%smbedtls-probe.log", app_dir) >= (int)sizeof(log_path) ||
        snprintf(ca_path, sizeof(ca_path), "%scacert.crt", app_dir) >= (int)sizeof(ca_path) ||
        snprintf(entropy_path, sizeof(entropy_path), "%sRAProbeEntropy.prx", app_dir) >= (int)sizeof(entropy_path)) return -1;
    return 0;
}
static void close_log(void)
{
    int rc;
    if (log_fd < 0) return;
    rc = sceIoClose(log_fd); log_fd = -1;
    if (rc < 0 && !log_data.error) log_data.error = rc;
    rc = sceIoSync("ms0:", 0);
    if (rc < 0 && !log_data.error) log_data.error = rc;
}
int main(int argc, char** argv)
{
    SceCtrlData buttons;
    unsigned previous = 0;
    int thread, start;
    pspDebugScreenInit();
    if (init_paths(argc, argv)) { pspDebugScreenPrintf("Invalid application path\n"); sceKernelDelayThread(3000000); sceKernelExitGame(); return 1; }
    thread = sceKernelCreateThread("Callbacks", callback_thread, 0x11, 0x1000, 0, NULL);
    if (thread >= 0) sceKernelStartThread(thread, 0, NULL);
    pspDebugScreenPrintf("Credential-free Mbed TLS probe 0.2 / TLS 1.2\n");
    pspDebugScreenPrintf("CA, hostname and expiry checks REQUIRED.\n");
    pspDebugScreenPrintf("Correct console date/time; enable Wi-Fi.\n");
    pspDebugScreenPrintf("UP/DOWN profile, LEFT/RIGHT endpoint.\nCROSS run, CIRCLE exit; HOME during run.\n");
    do {
        unsigned pressed;
        sceCtrlReadBufferPositive(&buttons, 1);
        pressed = buttons.Buttons & ~previous; previous = buttons.Buttons;
        if ((pressed & PSP_CTRL_UP) && profile < 32) ++profile;
        if ((pressed & PSP_CTRL_DOWN) && profile > 1) --profile;
        if (pressed & PSP_CTRL_RIGHT) endpoint = (endpoint + 1) % 6;
        if (pressed & PSP_CTRL_LEFT) endpoint = (endpoint + 5) % 6;
        if (pressed & PSP_CTRL_CIRCLE) { sceKernelExitGame(); return 0; }
        pspDebugScreenSetXY(0, 7);
        pspDebugScreenPrintf("Profile: %d endpoint: %u     \n%s              \n", profile, endpoint, hosts[endpoint]);
        sceKernelDelayThread(50000);
    } while (!(buttons.Buttons & PSP_CTRL_CROSS));
    log_fd = sceIoOpen(log_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0) { pspDebugScreenPrintf("Cannot write log: 0x%08X\n", (unsigned)log_fd); sceKernelDelayThread(3000000); sceKernelExitGame(); return 1; }
    ra_probe_log_init(&log_data, log_write, NULL);
    thread = sceKernelCreateThread("MbedTLS worker", worker, 0x18, 0x20000, 0, NULL);
    start = thread < 0 ? thread : sceKernelStartThread(thread, 0, NULL);
    if (start < 0) { record("create/start worker", start); close_log(); pspDebugScreenPrintf("Cannot start worker: 0x%08X\n", (unsigned)start); sceKernelDelayThread(3000000); sceKernelExitGame(); return 1; }
    while (!done) { pspDebugScreenSetXY(0, 11); pspDebugScreenPrintf("Running; HOME can exit.\n"); sceKernelDelayThread(100000); }
    sceKernelWaitThreadEnd(thread, NULL); sceKernelDeleteThread(thread); close_log();
    pspDebugScreenPrintf("%s code=0x%08X\n", outcome, (unsigned)result_code);
    pspDebugScreenPrintf("Stage: %s\nFirst code: 0x%08X\n", failure_stage, (unsigned)first_failure);
    pspDebugScreenPrintf("Verify flags: 0x%08X HTTP: %d\n", (unsigned)tls_result.verify_flags, tls_result.http_status);
    pspDebugScreenPrintf("TLS heap peak: %u remaining: %u bytes\n", (unsigned)tls_result.memory_peak, (unsigned)tls_result.memory_remaining);
    pspDebugScreenPrintf("Log I/O error: 0x%08X\n%s\n", (unsigned)log_data.error, log_path);
    pspDebugScreenPrintf("CROSS: exit. Save log before next run.\n");
    do { sceCtrlReadBufferPositive(&buttons, 1); sceKernelDelayThread(100000); } while (!(buttons.Buttons & PSP_CTRL_CROSS));
    sceKernelExitGame(); return 0;
}
