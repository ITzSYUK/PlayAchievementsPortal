#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psprtc.h>
#include <psppower.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/platform_util.h>
#include "tls_probe.h"
#include "rtc_epoch.h"
#include "probe_shared.h"
#include "probe_policy.h"
#include "probe_power.h"
#include "entropy_request.h"
#include "md5.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

PSP_MODULE_INFO("RAInGameWorker", 0, 0, 2);
PSP_HEAP_SIZE_KB(2048);
/* Explicit library initialization/termination: no CRT main/exit game path. */
extern void __libcglue_init(int, char**);
extern void __libcglue_deinit(void);
SceUID kuKernelLoadModule(const char*, int, SceKernelLMOption*);
static ra_ingame_shared* shared;
static SceUID worker_thread = -1, log_fd = -1;
static ra_probe_log log_data;
static unsigned log_bytes;
static ra_probe_power power_owner;
static int socket_fd = -1;
static u64 rtc_epoch_tick;
static u32 rtc_resolution;
static ra_entropy_output seed __attribute__((aligned(64)));
static size_t seed_offset;
static int log_write(const void*, unsigned, void*);
void _exit(int status) __attribute__((noreturn));
void _exit(int status)
{
    static const char message[] = "FATAL libc exit requested; probe stops without exiting the game\n";
    /* libc abort/assert must never reach CRT sceKernelExitGame. Contexts may
     * still be live: leave done=0 so module_stop refuses unsafe unload. */
    if (shared) { shared->worker_result = status ? status : -34007; shared->stop = 1; }
    if (log_fd >= 0) log_write(message, sizeof(message) - 1, NULL);
    sceKernelExitThread(status);
    for (;;) sceKernelDelayThread(100000);
}

static int cancelled(void)
{
    /* The callback is created by this thread, so service it here, including
     * between file/network calls. It only sets flags and never performs I/O. */
    sceKernelCheckCallback();
    return !shared || shared->stop || shared->suspended;
}
static int log_write(const void* data, unsigned n, void* unused)
{
    int rc;
    (void)unused;
    if (n > RA_INGAME_LOG_LIMIT - log_bytes) return -34002;
    rc = sceIoWrite(log_fd, data, n);
    if (rc > 0) log_bytes += (unsigned)rc;
    return rc;
}
static int record(const char* name, int rc)
{ ra_probe_log_printf(&log_data, "%s: 0x%08X\n", name, (unsigned)rc); return rc; }
static int power_event(int unused, int flags, void* context)
{
    (void)unused; (void)context;
    if ((unsigned)flags & (PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_POWER_SWITCH |
                          PSP_POWER_CB_RESUMING | PSP_POWER_CB_RESUME_COMPLETE)) {
        shared->suspended = 1;
        shared->stop = 1;
    }
    return 0;
}
static int power_create(void* unused)
{ (void)unused; return record("create user power callback", sceKernelCreateCallback("RAProbePower", power_event, NULL)); }
static int power_register(int slot, int callback, void* unused)
{ (void)unused; return record("register user power callback", scePowerRegisterCallback(slot, callback)); }
static int power_unregister(int slot, void* unused)
{ (void)unused; return record("unregister user power callback", scePowerUnregisterCallback(slot)); }
static int power_delete(int callback, void* unused)
{ (void)unused; return record("delete user power callback", sceKernelDeleteCallback(callback)); }
static const ra_probe_power_api power_api = {
    power_create, power_register, power_unregister, power_delete, NULL
};
static uint64_t now_ms(void* unused)
{ (void)unused; return sceKernelGetSystemTimeWide() / 1000; }
static time_t utc_time(void* unused)
{
    u64 tick;
    int64_t seconds;
    time_t result;
    (void)unused;
    if (sceRtcGetCurrentTick(&tick) < 0 || ra_rtc_epoch_seconds(tick, rtc_epoch_tick, rtc_resolution, &seconds)) return (time_t)-1;
    result = (time_t)seconds;
    return (int64_t)result == seconds ? result : (time_t)-1;
}
static int retryable(int e) { return e == EINTR || e == EWOULDBLOCK || e == EAGAIN; }
static int wait_io(void* unused, int writing, unsigned ms)
{
    SceNetInetPollfd fd = { socket_fd, writing ? SCE_NET_INET_POLLOUT : SCE_NET_INET_POLLIN, 0 };
    int rc;
    (void)unused;
    if (cancelled()) return RA_INGAME_CANCEL;
    rc = sceNetInetPoll(&fd, 1, (int)ms);
    return rc < 0 && sceNetInetGetErrno() != EINTR ? MBEDTLS_ERR_NET_RECV_FAILED : 0;
}
static int send_bio(void* unused, const unsigned char* bytes, size_t size)
{
    int rc, error;
    (void)unused;
    if (cancelled()) return RA_INGAME_CANCEL;
    rc = (int)sceNetInetSend(socket_fd, bytes, size, 0);
    if (rc >= 0) return rc;
    error = sceNetInetGetErrno();
    if (retryable(error)) return MBEDTLS_ERR_SSL_WANT_WRITE;
    record("send errno", error); return MBEDTLS_ERR_NET_SEND_FAILED;
}
static int recv_bio(void* unused, unsigned char* bytes, size_t size)
{
    int rc, error;
    (void)unused;
    if (cancelled()) return RA_INGAME_CANCEL;
    rc = (int)sceNetInetRecv(socket_fd, bytes, size, 0);
    if (rc >= 0) return rc;
    error = sceNetInetGetErrno();
    if (retryable(error)) return MBEDTLS_ERR_SSL_WANT_READ;
    record("receive errno", error); return MBEDTLS_ERR_NET_RECV_FAILED;
}
static int entropy(void* unused, unsigned char* bytes, size_t size)
{
    (void)unused;
    if (cancelled() || seed.status != 0 || size > sizeof(seed.bytes) - seed_offset)
        return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    memcpy(bytes, seed.bytes + seed_offset, size);
    mbedtls_platform_zeroize(seed.bytes + seed_offset, size); seed_offset += size;
    return 0;
}
static int seed_from_kirk(void)
{
    ra_entropy_request request = { &seed };
    SceUID module;
    int rc, status = -1, stop_status = -1, stop_result;
    memset(&seed, 0, sizeof(seed)); seed.status = -1; seed_offset = 0;
    if (cancelled()) return RA_INGAME_CANCEL;
    module = record("load KIRK helper", kuKernelLoadModule(RA_INGAME_DIR "RAProbeEntropy.prx", 0, NULL));
    if (module < 0) return module;
    sceKernelDcacheWritebackInvalidateRange(&seed, sizeof(seed));
    rc = record("start KIRK helper", sceKernelStartModule(module, sizeof(request), &request, &status, NULL));
    sceKernelDcacheInvalidateRange(&seed, sizeof(seed));
    record("helper start status", status);
    if (rc >= 0 && status == 0) {
        rc = seed.status;
        record("KIRK command 14", rc);
        stop_result = record("stop KIRK helper", sceKernelStopModule(module, 0, NULL, &stop_status, NULL));
        record("helper stop status", stop_status);
        if (stop_result < 0 || stop_status != 0) rc = -1;
    } else rc = rc < 0 ? rc : -1;
    if (record("unload KIRK helper", sceKernelUnloadModule(module)) < 0) rc = -1;
    return rc > 0 ? MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED : rc;
}
static int read_profile(void)
{
    SceUID fd = sceIoOpen(RA_INGAME_DIR "profile.txt", PSP_O_RDONLY, 0);
    char data[8] = {0};
    unsigned value = 0;
    int size, i;
    if (fd < 0) return fd;
    size = sceIoRead(fd, data, sizeof(data)); sceIoClose(fd);
    if (size <= 0 || size >= (int)sizeof(data)) return -1;
    for (i = 0; i < size && data[i] >= '0' && data[i] <= '9'; ++i) value = value * 10 + data[i] - '0';
    if (!i || value < 1 || value > 32) return -1;
    for (; i < size; ++i) if (data[i] != '\r' && data[i] != '\n') return -1;
    return (int)value;
}
static int run_https(int profile)
{
    const int modules[] = { PSP_NET_MODULE_COMMON, PSP_NET_MODULE_INET };
    unsigned loaded = 0;
    int net = 0, inet = 0, resolver = 0, apctl = 0, connected = 0, rid = -1;
    int rc = -1, state, previous_state = -1, nonblock = 1, socket_error = 0, cleanup_error = 0;
    socklen_t error_size = sizeof(socket_error);
    struct sockaddr_in address;
    const ScePspDateTime epoch = {1970, 1, 1, 0, 0, 0, 0};
    uint64_t started;
    char dns_buffer[1024];
    ra_tls_options options;
    ra_tls_result result;
    memset(&result, 0, sizeof(result));
#define STEP(label, call) do { if (cancelled()) { rc = RA_INGAME_CANCEL; goto end; } \
    ra_probe_log_printf(&log_data, "begin: %s\n", label); rc = record(label, (call)); if (rc < 0) goto end; } while (0)
#define CLEAN(label, call) do { int cleanup_rc = record(label, (call)); if (cleanup_rc < 0 && !cleanup_error) cleanup_error = cleanup_rc; } while (0)
    /* Bootstrap checks game modules immediately before the button request.
     * Failed init is not treated as ownership of an already running stack. */
    if (!ra_probe_network_allowed(shared->network_present, shared->stop, shared->suspended)) {
        ra_probe_log_printf(&log_data, "HTTPS_SKIPPED: foreign network modules or cancellation; game stack not touched\n");
        return -34004;
    }
    STEP("RTC epoch anchor", sceRtcGetTick(&epoch, &rtc_epoch_tick));
    rtc_resolution = sceRtcGetTickResolution();
    if (!rtc_resolution || utc_time(NULL) == (time_t)-1) { rc = RA_TLS_CLOCK_ERROR; goto end; }
    STEP("KIRK entropy", seed_from_kirk());
    for (loaded = 0; loaded < 2; ++loaded) STEP("load owned network module", sceUtilityLoadNetModule(modules[loaded]));
    STEP("owned net init", sceNetInit(0x20000, 0x2A, 0, 0x2A, 0)); net = 1;
    STEP("owned inet init", sceNetInetInit()); inet = 1;
    STEP("owned resolver init", sceNetResolverInit()); resolver = 1;
    STEP("owned apctl init", sceNetApctlInit(0x1800, 0x30)); apctl = 1;
    STEP("connect profile", sceNetApctlConnect(profile)); connected = 1;
    started = now_ms(NULL);
    while (1) {
        STEP("APCTL state", sceNetApctlGetState(&state));
        if (state != previous_state) {
            ra_probe_log_printf(&log_data, "apctl_state=%d elapsed_ms=%u\n", state, (unsigned)(now_ms(NULL) - started));
            previous_state = state;
        }
        if (state == PSP_NET_APCTL_STATE_GOT_IP) break;
        if (now_ms(NULL) - started > 30000) { rc = RA_TLS_TIMEOUT; goto end; }
        sceKernelDelayThread(250000);
    }
    memset(&address, 0, sizeof(address)); address.sin_family = AF_INET; address.sin_port = htons(443);
    STEP("create resolver", sceNetResolverCreate(&rid, dns_buffer, sizeof(dns_buffer)));
    STEP("bounded DNS", sceNetResolverStartNtoA(rid, "retroachievements.org", &address.sin_addr, 7, 1));
    STEP("create TCP socket", sceNetInetSocket(AF_INET, SOCK_STREAM, 0)); socket_fd = rc;
    STEP("nonblocking TCP", sceNetInetSetsockopt(socket_fd, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock)));
    rc = sceNetInetConnect(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (rc < 0) {
        int error = sceNetInetGetErrno();
        record("connect errno", error);
        if (error != EINPROGRESS && error != EALREADY && !retryable(error)) { rc = MBEDTLS_ERR_NET_CONNECT_FAILED; goto end; }
        started = now_ms(NULL);
        while (1) {
            SceNetInetPollfd pollfd = {socket_fd, SCE_NET_INET_POLLOUT, 0};
            if (cancelled()) { rc = RA_INGAME_CANCEL; goto end; }
            if (now_ms(NULL) - started > 7000) { rc = RA_TLS_TIMEOUT; goto end; }
            rc = sceNetInetPoll(&pollfd, 1, 100);
            if (rc < 0) goto end;
            if (rc > 0) break;
        }
        STEP("TCP SO_ERROR", sceNetInetGetsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size));
        if (socket_error) { record("TCP connect errno", socket_error); rc = MBEDTLS_ERR_NET_CONNECT_FAILED; goto end; }
    }
    memset(&options, 0, sizeof(options));
    options.send = send_bio; options.recv = recv_bio; options.wait_io = wait_io;
    options.entropy = entropy; options.now_ms = now_ms; options.utc_time = utc_time;
    options.hostname = "retroachievements.org"; options.path = "/dorequest.php";
    options.ca_file = RA_INGAME_DIR "cacert.crt"; options.timeout_ms = 45000; options.log = &log_data;
    started = now_ms(NULL);
    ra_probe_log_printf(&log_data, "user_free_before_tls=%u max_block=%u\n", (unsigned)sceKernelTotalFreeMemSize(), (unsigned)sceKernelMaxFreeMemSize());
    rc = ra_tls_probe(&options, &result);
    ra_probe_log_printf(&log_data, "ingame_TLS_code=%d HTTP=%d elapsed_ms=%u user_free_after_tls=%u\n", rc, result.http_status,
        (unsigned)(now_ms(NULL) - started), (unsigned)sceKernelTotalFreeMemSize());
    if (rc == 0) ra_probe_log_printf(&log_data, "TLS_VERIFIED_IN_GAME (HTTP status recorded separately; no API body parsed)\n");
end:
    mbedtls_platform_zeroize(&seed, sizeof(seed));
    if (socket_fd >= 0) { CLEAN("close TCP", sceNetInetClose(socket_fd)); socket_fd = -1; }
    if (rid >= 0) CLEAN("delete resolver", sceNetResolverDelete(rid));
    if (connected) CLEAN("disconnect owned APCTL", sceNetApctlDisconnect());
    if (apctl) CLEAN("term owned APCTL", sceNetApctlTerm());
    if (resolver) CLEAN("term owned resolver", sceNetResolverTerm());
    if (inet) CLEAN("term owned inet", sceNetInetTerm());
    if (net) CLEAN("term owned net", sceNetTerm());
    while (loaded) { --loaded; CLEAN("unload owned network module", sceUtilityUnloadNetModule(modules[loaded])); }
    if (rc == 0 && cleanup_error) rc = cleanup_error;
    if (result.memory_remaining) rc = -34005;
    shared->http_status = result.http_status; shared->tls_result = rc;
    ra_probe_log_printf(&log_data, "HTTPS_finished code=%d cleanup_error=%d\n", rc, cleanup_error);
    return rc;
#undef STEP
#undef CLEAN
}
static void digest_text(const unsigned char* digest, char* output)
{
    const char* hex = "0123456789abcdef";
    unsigned i;
    for (i = 0; i < 16; ++i) { output[2*i] = hex[digest[i] >> 4]; output[2*i+1] = hex[digest[i] & 15]; }
    output[32] = 0;
}
static int hash_file(const char* path, md5_state_t* combined)
{
    FILE* file = fopen(path, "rb");
    md5_state_t md5;
    unsigned char buffer[2048], digest[16], prefix[4] = {0};
    char hash[33];
    long size, remaining, hashed = 0;
    int rc = -1;
    if (!file) { ra_probe_log_printf(&log_data, "file unavailable: %s errno=%d\n", path, errno); return -1; }
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) goto end;
    remaining = size > 64L * 1024L * 1024L ? 64L * 1024L * 1024L : size;
    md5_init(&md5);
    while (remaining) {
        size_t n = remaining > (long)sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
        if (cancelled()) { rc = RA_INGAME_CANCEL; goto end; }
        if (fread(buffer, 1, n, file) != n || ferror(file)) goto end;
        if (!hashed && n >= sizeof(prefix)) memcpy(prefix, buffer, sizeof(prefix));
        md5_append(&md5, buffer, (int)n); md5_append(combined, buffer, (int)n);
        remaining -= (long)n; hashed += (long)n;
        /* Yield while the game uses the same storage, about 1.1s for this EBOOT. */
        sceKernelDelayThread(1000);
    }
    md5_finish(&md5, digest); digest_text(digest, hash);
    ra_probe_log_printf(&log_data, "disc_file=%s size=%ld hashed=%ld md5=%s magic=%02X%02X%02X%02X\n",
        path, size, hashed, hash, prefix[0], prefix[1], prefix[2], prefix[3]);
    rc = 0;
end:
    if (fclose(file) && rc == 0) rc = -1;
    return rc;
}
static int check_disc_hash(void)
{
    md5_state_t combined;
    unsigned char digest[16];
    char hash[33];
    int rc;
    md5_init(&combined);
    rc = hash_file("disc0:/PSP_GAME/PARAM.SFO", &combined);
    if (rc == 0) rc = hash_file("disc0:/PSP_GAME/SYSDIR/EBOOT.BIN", &combined);
    if (rc != 0) { ra_probe_log_printf(&log_data, "DISC_HASH_UNAVAILABLE code=%d (no reference substituted)\n", rc); return rc; }
    md5_finish(&combined, digest); digest_text(digest, hash);
    ra_probe_log_printf(&log_data, "disc0_candidate_hash=%s pc_reference=%s\n", hash, RA_INGAME_EXPECTED_HASH);
    ra_probe_log_printf(&log_data, "%s (diagnostic comparison only; no game identity sent to server)\n",
        strcmp(hash, RA_INGAME_EXPECTED_HASH) == 0 ? "DISC_HASH_MATCH" : "DISC_HASH_MISMATCH");
    return strcmp(hash, RA_INGAME_EXPECTED_HASH) == 0 ? 0 : -34006;
}
static int worker(SceSize args, void* argp)
{
    char* argv[] = { RA_INGAME_DIR "RAInGameWorker.prx", NULL };
    int profile, power_result = 0;
    (void)args; (void)argp;
    ra_probe_power_init(&power_owner);
    __libcglue_init(1, argv);
    log_fd = sceIoOpen(RA_INGAME_DIR "worker.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0) { shared->worker_result = log_fd; goto done; }
    ra_probe_log_init(&log_data, log_write, NULL);
    ra_probe_log_printf(&log_data, "probe_version=0.2 user worker heap_kib=2048 target=%s\n", RA_INGAME_UMD);
    power_result = ra_probe_power_register(&power_owner, &power_api);
    if (power_result < 0) {
        shared->worker_result = power_result;
        ra_probe_log_printf(&log_data, "POWER_CALLBACK_UNAVAILABLE; hash/TLS not started\n");
        goto done;
    }
    profile = record("profile index", read_profile());
    if (profile < 1) { shared->worker_result = -1; goto done; }
    shared->worker_result = record("disc comparison", check_disc_hash());
    shared->ready = 1;
    ra_probe_log_printf(&log_data, "worker_ready: L+R+SQUARE requests one credential-free HTTPS operation\n");
    sceIoSync("ms0:", 0);
    while (!cancelled()) {
        if (shared->tls_request && !shared->tls_done) {
            shared->tls_result = run_https(profile);
            shared->tls_done = 1;
            sceIoSync("ms0:", 0);
        }
        sceKernelDelayThreadCB(20000);
    }
done:
    shared->stop = 1;
    power_result = ra_probe_power_release(&power_owner, &power_api);
    if (power_result < 0) {
        shared->worker_result = power_result;
        ra_probe_log_printf(&log_data, "power callback cleanup failed; module/shared block must remain allocated\n");
    }
    if (log_fd >= 0) {
        ra_probe_log_printf(&log_data, "worker_finished stop=%u suspended=%u log_error=0x%08X\n", (unsigned)shared->stop, (unsigned)shared->suspended, (unsigned)log_data.error);
        sceIoClose(log_fd); log_fd = -1; sceIoSync("ms0:", 0);
    }
    __libcglue_deinit();
    /* module_stop refuses unload when a callback has not been released. */
    shared->worker_done = power_result == 0;
    return 0;
}
int module_start(SceSize args, void* argp)
{
    ra_ingame_args request;
    uintptr_t pointer;
    int rc;
    if (args != sizeof(request) || !argp) return -1;
    memcpy(&request, argp, sizeof(request)); pointer = (uintptr_t)request.shared;
    if (pointer < 0x08800000u || pointer > 0x0C000000u - sizeof(*shared) || pointer % 4) return -1;
    shared = request.shared;
    if (shared->magic != RA_INGAME_MAGIC || shared->version != 1 || shared->size != sizeof(*shared)) return -1;
    worker_thread = sceKernelCreateThread("RAInGameTLS", worker, 0x38, 0x20000, PSP_THREAD_ATTR_USER, NULL);
    if (worker_thread < 0) return worker_thread;
    rc = sceKernelStartThread(worker_thread, 0, NULL);
    if (rc < 0) { sceKernelDeleteThread(worker_thread); worker_thread = -1; }
    return rc;
}
int module_stop(SceSize args, void* argp)
{
    SceUInt timeout = 10000000;
    (void)args; (void)argp;
    if (shared) shared->stop = 1;
    if (worker_thread < 0) return 0;
    /* Never unload a live worker or free its shared control behind it. */
    if (sceKernelWaitThreadEnd(worker_thread, &timeout) < 0 || !shared || !shared->worker_done) return -1;
    sceKernelDeleteThread(worker_thread); worker_thread = -1;
    return 0;
}
