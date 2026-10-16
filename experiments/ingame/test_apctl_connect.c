#define RA_CLIENT_HTTPS 1
#include "client_shared.h"
#include "ra_net_resources.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
typedef int SceUID;
typedef unsigned SceUInt;
#define PSP_NET_APCTL_EVENT_ERROR 6
#define PSP_NET_APCTL_STATE_DISCONNECTED 0
static ra_client_control shared;
static ra_client_control* control = &shared;
static ra_net_resources network;
static struct { int error; } logger;
static struct {
    int wlan, power, profile, create_error, add_error, connect_error, state, wait_error, delete_error, log_fail;
    unsigned connects, sem_creates, sem_deletes, handler_deletes, disconnects;
    int locked, handler_registered, inside_callback;
    void (*handler)(int, int, int, int, void*);
    char log[8192]; unsigned length;
} mock;
static uint64_t now_ms(void* context) { (void)context; return 12345; }
static int check(void) { return control->stop ? -35204 : 0; }
static void memory_mark(const char* label, const char* phase) { (void)label; (void)phase; }
static int ra_probe_log_printf(void* unused, const char* format, ...)
{
    va_list args; int size; (void)unused;
    assert(!mock.inside_callback); /* The firmware handler must never write logs. */
    va_start(args, format); size = vsnprintf(mock.log + mock.length, sizeof(mock.log) - mock.length, format, args); va_end(args);
    assert(size >= 0 && (unsigned)size < sizeof(mock.log) - mock.length);
    mock.length += (unsigned)size; return size;
}
static int record(const char* label, int rc)
{
    if (mock.log_fail && strstr(label, "(deferred)")) logger.error = -91;
    ra_probe_log_printf(NULL, "%s: %d\n", label, rc); return rc;
}
static int sceKernelCreateSema(const char* name, int attr, int initial, int maximum, void* option)
{
    assert(!strcmp(name, "RAApctlEvents") && !attr && initial == 1 && maximum == 1 && !option);
    ++mock.sem_creates; return mock.create_error ? mock.create_error : 37;
}
static int sceKernelWaitSema(int uid, int count, SceUInt* timeout)
{
    assert(uid == 37 && count == 1 && *timeout == 100000);
    if (mock.wait_error) return mock.wait_error;
    assert(!mock.locked); mock.locked = 1; return 0;
}
static int sceKernelSignalSema(int uid, int count)
{ assert(uid == 37 && count == 1 && mock.locked); mock.locked = 0; return 0; }
static int sceKernelDeleteSema(int uid)
{
    assert(uid == 37 && !mock.locked && !network.apctl && !network.modules && network.apctl_handler == -1);
    ++mock.sem_deletes; return mock.delete_error;
}
static int sceNetApctlAddHandler(void (*handler)(int, int, int, int, void*), void* context)
{
    assert(!context); mock.handler = handler;
    if (mock.add_error) return mock.add_error;
    mock.handler_registered = 1; return 0;
}
static int sceWlanGetSwitchState(void) { return mock.wlan; }
static int sceWlanDevIsPowerOn(void) { return mock.power; }
static int sceUtilityCheckNetParam(int wifi) { assert(wifi == 1); return mock.profile; }
static int sceNetApctlGetState(int* state) { *state = mock.state; return 0; }
static int sceNetApctlConnect(int wifi)
{ assert(wifi == 1 && mock.handler_registered && control->request_allowed); ++mock.connects; return mock.connect_error; }
#include "../psp/ingame/https/user/apctl_observer.h"
#include "../psp/ingame/https/user/connect_profile.h"
static void emit(int event, int error)
{
    assert(mock.handler_registered); mock.inside_callback = 1;
    mock.handler(1, 0, event, error, NULL); mock.inside_callback = 0;
}
static int native_release(void* unused, int op, int arg)
{
    (void)unused;
    if (op == RA_NET_DISCONNECT) { ++mock.disconnects; emit(5, 0); }
    if (op == RA_NET_DELETE_APCTL_HANDLER) {
        assert(arg == 0 && mock.handler_registered); mock.handler_registered = 0; ++mock.handler_deletes;
    }
    if (op == RA_NET_APCTL_TERM) assert(!mock.handler_registered);
    return 0;
}
static int cleanup(void)
{
    int rc = ra_net_resources_release(&network, native_release, NULL);
    return release_apctl_observer(rc);
}
static void reset(void)
{
    memset(&shared, 0, sizeof(shared)); memset(&mock, 0, sizeof(mock)); logger.error = 0;
    shared.request_allowed = 1; mock.wlan = 1; mock.power = 0; /* On-demand power is allowed. */
    apctl_event_lock = -1; apctl_guard_error = 0; apctl_dropped_reported = 0;
    ra_net_resources_init(&network); network.modules = 2; network.net = network.inet = network.resolver = network.apctl = 1;
}
int main(void)
{
    unsigned i;
    reset(); assert(connect_profile(1) == 0 && mock.connects == 1 && network.connected);
    emit(1, 0); assert(!strstr(mock.log, "APCTL event uptime"));
    assert(drain_apctl_events() == 0 && strstr(mock.log, "uptime_ms=12345 old=1 new=0 event=1 error=0x00000000"));
    assert(cleanup() == 0 && mock.disconnects == 1 && mock.handler_deletes == 1 && mock.sem_deletes == 1);
    reset(); shared.request_allowed = 0;
    assert(connect_profile(1) == -35322 && mock.connects == 0 && mock.sem_creates == 0);
    reset(); mock.wlan = 0;
    assert(connect_profile(1) == -35318 && mock.connects == 0 && mock.sem_creates == 0);
    reset(); mock.profile = -1;
    assert(connect_profile(1) == -35319 && mock.sem_creates == 0);
    reset(); mock.state = 3;
    assert(connect_profile(1) == -35320 && mock.connects == 0); assert(cleanup() == 0);
    reset(); mock.create_error = -66;
    assert(connect_profile(1) == -66 && mock.connects == 0); assert(cleanup() == 0 && mock.sem_deletes == 0);
    reset(); mock.add_error = -67;
    assert(connect_profile(1) == -67 && mock.connects == 0); assert(cleanup() == 0 && mock.sem_deletes == 1);
    reset(); mock.connect_error = -68;
    assert(connect_profile(1) == -68 && !network.connected); assert(cleanup() == 0 && mock.disconnects == 0);
    reset(); mock.log_fail = 1;
    assert(connect_profile(1) == -91 && network.connected); assert(cleanup() == 0 && mock.disconnects == 1);
    reset(); assert(connect_profile(1) == 0);
    for (i = 0; i < RA_APCTL_EVENTS_MAX; ++i) emit(1, 0);
    emit(PSP_NET_APCTL_EVENT_ERROR, -88);
    assert(drain_apctl_events() == -88 && strstr(mock.log, "event_history_dropped=1"));
    assert(drain_apctl_events() == -88); assert(cleanup() == 0);
    reset(); assert(connect_profile(1) == 0); emit(PSP_NET_APCTL_EVENT_ERROR, 0);
    assert(drain_apctl_events() == -35321); assert(cleanup() == 0);
    reset(); assert(connect_profile(1) == 0); mock.wait_error = -69;
    assert(ra_net_resources_release(&network, native_release, NULL) == 0);
    assert(release_apctl_observer(0) == -69 && mock.sem_deletes == 0 && apctl_event_lock == 37);
    reset(); assert(connect_profile(1) == 0);
    assert(release_apctl_observer(-77) == -77 && mock.sem_deletes == 0);
    assert(ra_net_resources_release(&network, native_release, NULL) == 0);
    mock.delete_error = -70;
    assert(release_apctl_observer(0) == -70 && apctl_event_lock == 37);
    mock.delete_error = 0; assert(release_apctl_observer(0) == 0 && apctl_event_lock == -1);
    puts("PASS deferred APCTL: switch/profile/state checks, handler-before-connect, owner-only log, bounded history/error latch, cleanup lifetime");
    return 0;
}
