#ifndef RA_HTTPS_CONNECT_PROFILE_H
#define RA_HTTPS_CONNECT_PROFILE_H
/* After request release and hash verification. No credentials or profile
 * strings are read. Handler ownership is established before Connect. */
static int connect_profile(int wifi)
{
    int rc, state;
    if (!control->request_allowed) return -35322;
    rc = check(); if (rc) return rc;
    rc = record("WLAN switch before Connect", sceWlanGetSwitchState());
    record("WLAN power before Connect (0 may be on-demand)", sceWlanDevIsPowerOn());
    if (rc != 1) return -35318;
    rc = record("configured WiFi profile exists", sceUtilityCheckNetParam(wifi));
    if (rc) return -35319;
    rc = start_apctl_events(); if (rc < 0) return rc;
    rc = sceNetApctlGetState(&state); record("APCTL GetState before Connect", rc);
    if (rc < 0) return rc;
    record("APCTL state before Connect", state);
    if (state != PSP_NET_APCTL_STATE_DISCONNECTED) return -35320;
    rc = check(); if (rc) return rc;
    memory_mark("connect configured profile (deferred)", "before");
    if (logger.error) return logger.error;
    rc = sceNetApctlConnect(wifi);
    if (rc >= 0) network.connected = 1;
    record("connect configured profile (deferred)", rc);
    memory_mark("connect configured profile (deferred)", "after");
    return rc < 0 ? rc : logger.error;
}
#endif
