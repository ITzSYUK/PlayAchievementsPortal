#define RA_CLIENT_HTTPS 1
#include "client_shared.h"
#include "ra_net_resources.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static ra_client_control shared;
static ra_client_control* control = &shared;
static ra_net_resources network;
static struct { int error; } logger;
static const int modules[] = { 1, 3 };
static uint64_t deadline;
static struct {
    int fail_call, low_after, log_fail_call, fail_release, cancel_wait, timeout_wait, fragmented;
    unsigned calls, releases, waits, release_mask;
    uint64_t now;
} mock;
static int acquire(void)
{ ++mock.calls; return (int)mock.calls == mock.fail_call ? -71 : 0; }
static unsigned sceKernelTotalFreeMemSize(void)
{ return (int)mock.calls >= mock.low_after ? 864512 : 29379072; }
static unsigned sceKernelMaxFreeMemSize(void) { return mock.fragmented ? 1048576 : sceKernelTotalFreeMemSize(); }
static uint64_t now_ms(void* unused) { (void)unused;return mock.now; }
static int check(void) { return control->stop ? -35204 : now_ms(NULL)>=deadline ? -35203 : 0; }
static int ra_probe_log_printf(void* log, const char* format, ...)
{ (void)log; (void)format; return 0; }
static void memory_mark(const char* label, const char* phase)
{ (void)label; (void)phase; }
static int record(const char* label, int rc)
{ (void)label; if ((int)mock.calls == mock.log_fail_call) logger.error = -91; return rc; }
static int sceUtilityLoadNetModule(int module)
{ assert(module == modules[mock.calls]); return acquire(); }
static int sceNetInit(int pool, int cp, int cs, int ip, int is)
{ assert(pool == 65536 && cp == 0x2A && ip == 0x2A && cs == 0 && is == 0); return acquire(); }
static int sceNetInetInit(void) { return acquire(); }
static int sceNetResolverInit(void) { return acquire(); }
static int sceNetApctlInit(int stack, int priority)
{ assert(stack == 6144 && priority == 0x30); return acquire(); }

static int sceKernelDelayThread(unsigned us)
{
    assert(us == 100000 && network.modules == 2 && network.net && network.inet && network.resolver && network.apctl && !network.connected);
    assert(network.socket == -1 && network.resolver_id == -1 && control->ready);
    mock.now+=mock.timeout_wait ? 90000 : 100;
    if (++mock.waits == 3) { if (mock.cancel_wait) control->stop = 1; else control->request_allowed = 1; }
    return 0;
}
#include "../psp/ingame/https/user/early_network.h"
static int release(void* context, int operation, int argument)
{
    (void)context; (void)argument; ++mock.releases;
    if (operation == mock.fail_release) return -81;
    mock.release_mask |= 1u << operation; return 0;
}
static void reset(void)
{
    memset(&shared, 0, sizeof(shared)); memset(&mock, 0, sizeof(mock)); logger.error = 0;
    mock.low_after = 100; mock.fail_release = -1; ra_net_resources_init(&network);
    deadline=20000;
}
int main(void)
{
    unsigned i;
    reset(); assert(prepare_early_network() == 0 && shared.ready && mock.calls == 6);
    assert(wait_request_release() == 0 && mock.waits == 3 && mock.releases == 0);
    assert(ra_net_resources_release(&network, release, NULL) == 0 && network.modules == 0);
    assert(mock.releases == 6); /* apctl, resolver, inet, net, two modules; no Connect during hold */
    for (i = 1; i <= 6; ++i) {
        reset(); mock.fail_call = (int)i;
        assert(prepare_early_network() == -71 && !shared.ready && mock.calls == i);
        assert(ra_net_resources_release(&network, release, NULL) == 0 && network.modules == 0);
        assert(mock.releases == i - 1);
        /* Post-call log failure still owns the successful acquisition. */
        reset(); mock.log_fail_call = (int)i;
        assert(prepare_early_network() == -91 && mock.calls == i && !shared.ready);
        assert(ra_net_resources_release(&network, release, NULL) == 0 && network.modules == 0);
        assert(mock.releases == i);
    }
    for (i = 0; i < 6; ++i) {
        reset(); mock.low_after = (int)i;
        assert(prepare_early_network() == -35317 && mock.calls == i && !shared.ready);
        assert(ra_net_resources_release(&network, release, NULL) == 0);
        assert(mock.releases == i);
    }
    reset(); mock.fragmented = 1;
    assert(prepare_early_network() == -35317 && mock.calls == 0);
    reset(); shared.stop = 1;
    assert(prepare_early_network() == -35204 && mock.calls == 0);
    reset(); assert(prepare_early_network() == 0); mock.timeout_wait = 1;
    assert(wait_request_release() == -35203 && mock.releases == 0 && mock.waits == 2);
    assert(ra_net_resources_release(&network, release, NULL) == 0);
    reset(); assert(prepare_early_network() == 0);mock.now=25000;
    assert(wait_request_release()==0 && mock.waits==3 && deadline==205000);
    assert(ra_net_resources_release(&network,release,NULL)==0);
    reset(); assert(prepare_early_network() == 0); mock.cancel_wait = 1;
    assert(wait_request_release() == -35204 && mock.releases == 0);
    mock.fail_release = RA_NET_APCTL_TERM;
    assert(ra_net_resources_release(&network, release, NULL) == -81 && network.modules == 2 && network.net && network.apctl);
    mock.fail_release = -1;
    assert(ra_net_resources_release(&network, release, NULL) == 0 && network.modules == 0);
    puts("PASS early network: every acquisition/log failure and late memory rejection cleaned, resources held through release barrier, cancel/retry");
    return 0;
}
