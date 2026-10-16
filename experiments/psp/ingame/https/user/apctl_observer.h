#ifndef RA_HTTPS_APCTL_OBSERVER_H
#define RA_HTTPS_APCTL_OBSERVER_H
#include "apctl_events.h"
static ra_apctl_events apctl_history;
static SceUID apctl_event_lock = -1;
static volatile int apctl_guard_error;
static unsigned apctl_dropped_reported;
static void apctl_event(int old_state, int new_state, int event, int error, void* unused)
{
    SceUInt timeout = 100000;
    ra_apctl_event item;
    int rc; (void)unused;
    rc = sceKernelWaitSema(apctl_event_lock, 1, &timeout);
    if (rc < 0) { apctl_guard_error = rc; return; }
    item.when_ms = (uint32_t)now_ms(NULL); item.old_state = old_state;
    item.new_state = new_state; item.event = event; item.error = error;
    ra_apctl_events_push(&apctl_history, &item);
    if (event == PSP_NET_APCTL_EVENT_ERROR && !apctl_history.first_error)
        apctl_history.first_error = -35321;
    rc = sceKernelSignalSema(apctl_event_lock, 1);
    if (rc < 0) apctl_guard_error = rc;
}
static int drain_apctl_events(void)
{
    ra_apctl_event items[RA_APCTL_EVENTS_MAX];
    SceUInt timeout = 100000;
    unsigned count = 0, i, dropped;
    int rc, first_error;
    if (apctl_event_lock < 0) return 0;
    rc = sceKernelWaitSema(apctl_event_lock, 1, &timeout);
    if (rc < 0) { apctl_guard_error = rc; return rc; }
    while (count < RA_APCTL_EVENTS_MAX && ra_apctl_events_pop(&apctl_history, &items[count])) ++count;
    first_error = apctl_history.first_error; dropped = apctl_history.dropped;
    rc = sceKernelSignalSema(apctl_event_lock, 1);
    if (rc < 0) apctl_guard_error = rc;
    /* Only this owner writes the log; the native handler just publishes data. */
    for (i = 0; i < count; ++i)
        ra_probe_log_printf(&logger, "APCTL event uptime_ms=%u old=%d new=%d event=%d error=0x%08X\n",
            (unsigned)items[i].when_ms, items[i].old_state, items[i].new_state, items[i].event, (unsigned)items[i].error);
    if (dropped != apctl_dropped_reported) {
        ra_probe_log_printf(&logger, "APCTL event_history_dropped=%u first_error=0x%08X\n", dropped, (unsigned)first_error);
        apctl_dropped_reported = dropped;
    }
    return apctl_guard_error ? apctl_guard_error : first_error < 0 ? first_error : first_error ? -35321 : 0;
}
static int start_apctl_events(void)
{
    int rc;
    ra_apctl_events_init(&apctl_history); apctl_guard_error = 0; apctl_dropped_reported = 0;
    apctl_event_lock = sceKernelCreateSema("RAApctlEvents", 0, 1, 1, NULL);
    if (apctl_event_lock < 0) return record("create APCTL event guard", apctl_event_lock);
    rc = sceNetApctlAddHandler(apctl_event, NULL);
    if (rc >= 0) network.apctl_handler = rc;
    record("register APCTL event handler", rc);
    return rc < 0 ? rc : logger.error;
}
static int release_apctl_observer(int network_cleanup)
{
    int rc;
    if (apctl_event_lock < 0) return network_cleanup;
    (void)drain_apctl_events();
    /* The handler is detached and APCTL terminated before freeing its guard.
     * Retain storage/module on any uncertain native cleanup or lock failure. */
    if (network_cleanup < 0) return network_cleanup;
    if (apctl_guard_error) return apctl_guard_error;
    rc = sceKernelDeleteSema(apctl_event_lock);
    record("delete APCTL event guard", rc);
    if (rc >= 0) apctl_event_lock = -1;
    return rc;
}
#endif
