#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psphttp.h>
#include <pspssl.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>

#include "probe_log.h"
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("RAHttpsProbe", 0, 0, 2);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);

#define TIMEOUT_US 7000000u
#define CONNECT_LIMIT_US 30000000u
#define REQUEST_LIMIT_US 45000000u
#define RESPONSE_LIMIT 32768u

/* Fixed, credential-free requests. No arbitrary endpoint or account input. */
static const char* const urls[] = {
    "https://retroachievements.org/dorequest.php",
    "https://sha256.badssl.com/",
    "https://expired.badssl.com/",
    "https://wrong.host.badssl.com/",
    "https://self-signed.badssl.com/",
    "https://ra-probe-no-such-host.invalid/"
};
static const int modules[] = {
    PSP_NET_MODULE_COMMON, PSP_NET_MODULE_INET, PSP_NET_MODULE_PARSEURI,
    PSP_NET_MODULE_PARSEHTTP, PSP_NET_MODULE_HTTP, PSP_NET_MODULE_SSL
};
static volatile int done;
static int profile = 1;
static unsigned endpoint;
static int probe_result;
static int first_failure;
static const char* failure_stage = "none";
static int last_http_status;
static int network_errno;
static int network_errno_available;
static SceUID log_fd = -1;
static char log_path[512];
static ra_probe_log probe_log;

static int log_write(const void* data, unsigned size, void* userdata)
{
    const SceUID fd = *(const SceUID*)userdata;
    return sceIoWrite(fd, data, size);
}

static int open_log(int argc, char** argv)
{
    const char* filename = "https-probe.log";
    const char* slash = argc > 0 && argv != NULL && argv[0] != NULL ? strrchr(argv[0], '/') : NULL;
    if (slash != NULL) {
        const size_t directory_size = (size_t)(slash - argv[0]) + 1;
        if (directory_size + strlen(filename) >= sizeof(log_path))
            return -1;
        memcpy(log_path, argv[0], directory_size);
        strcpy(log_path + directory_size, filename);
    }
    else {
        strcpy(log_path, "ms0:/PSP/GAME/RAHttpsProbe/https-probe.log");
    }
    log_fd = sceIoOpen(log_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0)
        return log_fd;
    ra_probe_log_init(&probe_log, log_write, &log_fd);
    return 0;
}

static void close_log(void)
{
    int result;
    if (log_fd < 0)
        return;
    result = sceIoClose(log_fd);
    log_fd = -1;
    if (result < 0 && probe_log.error == 0)
        probe_log.error = result;
    result = sceIoSync("ms0:", 0);
    if (result < 0 && probe_log.error == 0)
        probe_log.error = result;
}

static int record_result(const char* stage, int result)
{
    if (result < 0 && first_failure == 0) {
        first_failure = result;
        failure_stage = stage;
    }
    ra_probe_log_printf(&probe_log, "%s: 0x%08X\n", stage, (unsigned)result);
    return result;
}

static const char* apctl_state_name(int state)
{
    switch (state) {
    case PSP_NET_APCTL_STATE_DISCONNECTED: return "DISCONNECTED";
    case PSP_NET_APCTL_STATE_SCANNING: return "SCANNING";
    case PSP_NET_APCTL_STATE_JOINING: return "JOINING";
    case PSP_NET_APCTL_STATE_GETTING_IP: return "GETTING_IP";
    case PSP_NET_APCTL_STATE_GOT_IP: return "GOT_IP";
    case PSP_NET_APCTL_STATE_EAP_AUTH: return "EAP_AUTH";
    case PSP_NET_APCTL_STATE_KEY_EXCHANGE: return "KEY_EXCHANGE";
    default: return "UNKNOWN";
    }
}

static int exit_callback(int arg1, int arg2, void* common)
{
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void* argp)
{
    const int callback = sceKernelCreateCallback("Exit", exit_callback, NULL);
    (void)args; (void)argp;
    sceKernelRegisterExitCallback(callback);
    sceKernelSleepThreadCB();
    return 0;
}

static int https_worker(SceSize args, void* argp)
{
    unsigned loaded = 0;
    int net = 0, inet = 0, resolver = 0, apctl = 0, ssl = 0, http = 0, https = 0;
    int connected = 0, template_id = -1, connection_id = -1, request_id = -1;
    int result = -1, status = 0, state = 0;
    int previous_state = -1;
    unsigned total = 0, ssl_peak = 0;
    char buffer[1024];
    SceULong64 content_length = 0;
    SceUInt64 started;
    SceUInt64 last_report = 0;
    (void)args; (void)argp;

#define STEP(name, expression) do { ra_probe_log_printf(&probe_log, "begin: %s\n", name); \
    result = record_result(name, (expression)); if (result < 0) goto cleanup; } while (0)

    ra_probe_log_printf(&probe_log, "probe_version=0.2\nprofile_index=%d endpoint_index=%u\n", profile, endpoint);
    for (loaded = 0; loaded < sizeof(modules) / sizeof(modules[0]); ++loaded) {
        STEP("load net module", sceUtilityLoadNetModule(modules[loaded]));
    }
    STEP("net init", sceNetInit(0x20000, 0x2A, 0, 0x2A, 0)); net = 1;
    STEP("inet init", sceNetInetInit()); inet = 1;
    STEP("resolver init", sceNetResolverInit()); resolver = 1;
    STEP("apctl init", sceNetApctlInit(0x1800, 0x30)); apctl = 1;
    STEP("connect profile", sceNetApctlConnect(profile)); connected = 1;
    started = sceKernelGetSystemTimeWide();
    do {
        const SceUInt64 elapsed = sceKernelGetSystemTimeWide() - started;
        result = sceNetApctlGetState(&state);
        if (result < 0) {
            record_result("get apctl state", result);
            goto cleanup;
        }
        if (state != previous_state || elapsed - last_report >= 5000000u) {
            ra_probe_log_printf(&probe_log, "apctl_state=%d (%s) elapsed_ms=%u\n",
                state, apctl_state_name(state), (unsigned)(elapsed / 1000));
            previous_state = state;
            last_report = elapsed;
        }
        if (state == PSP_NET_APCTL_STATE_GOT_IP)
            break;
        if (elapsed >= CONNECT_LIMIT_US) {
            result = record_result("Wi-Fi deadline exceeded", -1);
            goto cleanup;
        }
        sceKernelDelayThread(250000);
    } while (1);

    STEP("ssl init", sceSslInit(0x28000)); ssl = 1;
    STEP("http init", sceHttpInit(0x25800)); http = 1;
    STEP("https init", sceHttpsInit(0, 0, 0, 0)); https = 1;
    STEP("load default certificates", sceHttpsLoadDefaultCert(0, 0));
    /* No certificate checks are disabled. Their actual behavior must be proved
     * by a working valid-cert control plus negative tests on the target PSP. */
    STEP("create template", sceHttpCreateTemplate("RA-PSP-Transport-Probe/0.2", PSP_HTTP_VERSION_1_1, 0));
    template_id = result;
    STEP("disable redirects", sceHttpDisableRedirect(template_id));
    STEP("resolver timeout", sceHttpSetResolveTimeOut(template_id, TIMEOUT_US));
    STEP("resolver retries", sceHttpSetResolveRetry(template_id, 1));
    STEP("connect timeout", sceHttpSetConnectTimeOut(template_id, TIMEOUT_US));
    STEP("send timeout", sceHttpSetSendTimeOut(template_id, TIMEOUT_US));
    STEP("receive timeout", sceHttpSetRecvTimeOut(template_id, TIMEOUT_US));
    STEP("header limit", sceHttpSetResHeaderMaxSize(template_id, 8192));
    STEP("create connection", sceHttpCreateConnectionWithURL(template_id, urls[endpoint], 0));
    connection_id = result;
    STEP("create request", sceHttpCreateRequestWithURL(connection_id, PSP_HTTP_METHOD_GET, (char*)urls[endpoint], 0));
    request_id = result;
    started = sceKernelGetSystemTimeWide();
    STEP("send HTTPS request", sceHttpSendRequest(request_id, NULL, 0));
    STEP("get status", sceHttpGetStatusCode(request_id, &status));
    last_http_status = status;
    ra_probe_log_printf(&probe_log, "http_status=%d\n", status);
    /* Negative endpoints succeeding at TLS is a failed validation experiment,
     * regardless of their HTTP status. A generic failure alone proves nothing. */
    if (endpoint >= 2) {
        result = record_result("UNEXPECTED negative endpoint accepted", -1);
        goto cleanup;
    }
    if (status < 200 || status >= 300) {
        result = record_result("non-success HTTP status", -1);
        goto cleanup;
    }
    if (sceHttpGetContentLength(request_id, &content_length) == 0 && content_length > RESPONSE_LIMIT) {
        result = record_result("content length over limit", -1);
        goto cleanup;
    }
    do {
        int received;
        if (sceKernelGetSystemTimeWide() - started >= REQUEST_LIMIT_US) {
            result = record_result("request deadline exceeded", -1);
            goto cleanup;
        }
        received = sceHttpReadData(request_id, buffer, sizeof(buffer));
        if (received < 0) {
            result = record_result("read response", received);
            goto cleanup;
        }
        if (received == 0)
            break;
        if ((unsigned)received > RESPONSE_LIMIT - total) {
            result = record_result("response over limit", -1);
            goto cleanup;
        }
        total += (unsigned)received;
    } while (1);
    ra_probe_log_printf(&probe_log, "response_bytes=%u\n", total);
    result = 0;

cleanup:
    /* Preserve the request failure before any secondary/cleanup diagnostics. */
    ra_probe_log_printf(&probe_log, "cleanup_begin first_failure_stage=%s code=0x%08X\n",
        failure_stage, (unsigned)first_failure);
    if (result < 0 && request_id >= 0) {
        const int diagnostic_result = sceHttpGetNetworkErrno(request_id, &network_errno);
        record_result("get network errno", diagnostic_result);
        network_errno_available = diagnostic_result == 0;
        if (network_errno_available)
            ra_probe_log_printf(&probe_log, "network_errno=%d\n", network_errno);
    }
    if (ssl) {
        const int diagnostic_result = sceSslGetUsedMemoryMax(&ssl_peak);
        record_result("get SSL peak memory", diagnostic_result);
        if (diagnostic_result == 0)
            ra_probe_log_printf(&probe_log, "ssl_peak_bytes=%u\n", ssl_peak);
    }
    if (request_id >= 0) {
        record_result("abort request", sceHttpAbortRequest(request_id));
        record_result("delete request", sceHttpDeleteRequest(request_id));
    }
    if (connection_id >= 0) record_result("delete connection", sceHttpDeleteConnection(connection_id));
    if (template_id >= 0) record_result("delete template", sceHttpDeleteTemplate(template_id));
    if (https) record_result("https end", sceHttpsEnd());
    if (http) record_result("http end", sceHttpEnd());
    if (ssl) record_result("ssl end", sceSslEnd());
    if (connected) record_result("apctl disconnect", sceNetApctlDisconnect());
    if (apctl) record_result("apctl term", sceNetApctlTerm());
    if (resolver) record_result("resolver term", sceNetResolverTerm());
    if (inet) record_result("inet term", sceNetInetTerm());
    if (net) record_result("net term", sceNetTerm());
    while (loaded > 0) { --loaded; record_result("unload net module", sceUtilityUnloadNetModule(modules[loaded])); }
    record_result("probe result (not a certificate diagnosis)", result);
    probe_result = result;
    done = 1;
    return 0;
}

int main(int argc, char** argv)
{
    SceCtrlData buttons;
    unsigned previous = 0;
    int thread;
    pspDebugScreenInit();
    thread = sceKernelCreateThread("Callbacks", callback_thread, 0x11, 0x1000, 0, NULL);
    if (thread >= 0) sceKernelStartThread(thread, 0, NULL);
    pspDebugScreenPrintf("Credential-free HTTPS capability probe 0.2\n");
    pspDebugScreenPrintf("Correct the console date/time before testing.\n");
    pspDebugScreenPrintf("UP/DOWN: profile index 1..32\nLEFT/RIGHT: endpoint\n");
    pspDebugScreenPrintf("CROSS: one request; CIRCLE: exit.\n");
    do {
        unsigned pressed;
        sceCtrlReadBufferPositive(&buttons, 1);
        pressed = buttons.Buttons & ~previous;
        previous = buttons.Buttons;
        if ((pressed & PSP_CTRL_UP) && profile < 32) ++profile;
        if ((pressed & PSP_CTRL_DOWN) && profile > 1) --profile;
        if (pressed & PSP_CTRL_RIGHT) endpoint = (endpoint + 1) % (sizeof(urls) / sizeof(urls[0]));
        if (pressed & PSP_CTRL_LEFT) endpoint = (endpoint + 5) % 6;
        if (pressed & PSP_CTRL_CIRCLE) { sceKernelExitGame(); return 0; }
        pspDebugScreenSetXY(0, 7);
        pspDebugScreenPrintf("Profile: %d   endpoint: %u          \n", profile, endpoint);
        pspDebugScreenPrintf("%s                         \n", urls[endpoint]);
        sceKernelDelayThread(50000);
    } while (!(buttons.Buttons & PSP_CTRL_CROSS));
    thread = open_log(argc, argv);
    if (thread < 0) {
        pspDebugScreenPrintf("Cannot create log: 0x%08X\n%s\n", (unsigned)thread, log_path);
        sceKernelDelayThread(3000000);
        sceKernelExitGame();
        return 1;
    }
    thread = sceKernelCreateThread("HTTPS worker", https_worker, 0x18, 0x10000, 0, NULL);
    if (thread < 0 || sceKernelStartThread(thread, 0, NULL) < 0) {
        pspDebugScreenPrintf("Cannot start HTTPS worker.\n");
        close_log();
        sceKernelDelayThread(3000000);
        sceKernelExitGame();
        return 1;
    }
    while (!done) {
        pspDebugScreenSetXY(0, 11);
        pspDebugScreenPrintf("Running on network worker; HOME can exit.\n");
        sceKernelDelayThread(100000);
    }
    sceKernelWaitThreadEnd(thread, NULL);
    sceKernelDeleteThread(thread);
    close_log();
    pspDebugScreenPrintf("Request result: %s (0x%08X)\n", probe_result == 0 ? "SUCCESS" : "FAILURE", (unsigned)probe_result);
    pspDebugScreenPrintf("First error: %s\nCode: 0x%08X\n", failure_stage, (unsigned)first_failure);
    if (network_errno_available)
        pspDebugScreenPrintf("Network errno: %d\n", network_errno);
    pspDebugScreenPrintf("HTTP status: %d\nLog I/O error: 0x%08X\n", last_http_status, (unsigned)probe_log.error);
    pspDebugScreenPrintf("Log: %s\n", log_path);
    pspDebugScreenPrintf("CROSS: exit. Restart for the next endpoint.\n");
    do {
        sceCtrlReadBufferPositive(&buttons, 1);
        sceKernelDelayThread(100000);
    } while (!(buttons.Buttons & PSP_CTRL_CROSS));
    sceKernelExitGame();
    return 0;
}
