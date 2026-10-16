#ifndef RA_HTTPS_EARLY_NETWORK_H
#define RA_HTTPS_EARLY_NETWORK_H
/* Included by the worker and the host failure-injection test. Acquisition is
 * deliberately before disc hashing or APCTL polling. Record ownership before
 * any post-call I/O can fail. Never free a placeholder and race to reacquire it. */
static int prepare_early_network(void)
{
    int rc;
#define EARLY_ACQUIRE(label, call, own) do { \
    rc = check(); if (rc) { return rc; } \
    if ((unsigned)sceKernelTotalFreeMemSize() < RA_CLIENT_EARLY_FREE || \
        (unsigned)sceKernelMaxFreeMemSize() < RA_CLIENT_EARLY_FREE) { \
        ra_probe_log_printf(&logger, "EARLY_NETWORK_WINDOW_MISSED operation=%s\n", label); \
        return -35317; \
    } \
    memory_mark(label, "before"); if (logger.error) { return logger.error; } \
    rc = (call); if (rc >= 0) { own; } \
    record(label, rc); memory_mark(label, "after"); \
    if (rc < 0) { return rc; } \
    if (logger.error) { return logger.error; } \
} while (0)
    EARLY_ACQUIRE("load_net_COMMON", sceUtilityLoadNetModule(modules[0]), network.modules = 1);
    EARLY_ACQUIRE("load_net_INET", sceUtilityLoadNetModule(modules[1]), network.modules = 2);
    EARLY_ACQUIRE("owned net init 64KiB", sceNetInit(65536, 0x2A, 0, 0x2A, 0), network.net = 1);
    EARLY_ACQUIRE("owned inet init", sceNetInetInit(), network.inet = 1);
    EARLY_ACQUIRE("owned resolver init", sceNetResolverInit(), network.resolver = 1);
    EARLY_ACQUIRE("owned apctl init", sceNetApctlInit(6144, 0x30), network.apctl = 1);
    control->ready = 1;
    return 0;
#undef EARLY_ACQUIRE
}
static int wait_request_release(void)
{
    int rc;uint64_t started=now_ms(NULL);
    /* Network acquisition keeps its original 20s deadline. The following
     * read-only kernel audit has a separate bounded wait, with no token/HTTP. */
    deadline=started+RA_CLIENT_GATE_TIMEOUT_MS;
    ra_probe_log_printf(&logger,"AUDIT_GATE_WAIT limit_ms=%u\n",RA_CLIENT_GATE_TIMEOUT_MS);
    while (!control->request_allowed) {
        rc = check(); if (rc) {
            ra_probe_log_printf(&logger,"AUDIT_GATE_DONE result=%d elapsed_ms=%u\n",rc,(unsigned)(now_ms(NULL)-started));
            return rc;
        }
        sceKernelDelayThread(100000);
    }
    rc=check();
    ra_probe_log_printf(&logger,"AUDIT_GATE_DONE result=%d elapsed_ms=%u\n",rc,(unsigned)(now_ms(NULL)-started));
    return rc;
}
#endif
