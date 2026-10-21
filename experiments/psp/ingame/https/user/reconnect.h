#ifndef RA_WIFI_RECONNECT_H
#define RA_WIFI_RECONNECT_H
#include "reconnect_policy.h"
#include "ra_ui.h"
static ra_reconnect_policy wifi_policy;
static uint64_t wifi_poll_at;
static int wifi_enabled,wifi_profile,wifi_previous=-1;
static unsigned wifi_attempts;
static int wifi_last_wlan=-1;
static void wifi_tick(void)
{
    int state,rc,action,wlan;uint64_t now=now_ms(NULL),old_next=wifi_policy.next;
    if(!wifi_enabled || control->stop || now<wifi_poll_at)return;
    wifi_poll_at=now+250;
    rc=sceNetApctlGetState(&state);
    if(rc<0){record("WIFI_STATE_ERROR",rc);ui_network_error(8,0,RA_REASON_APCTL,rc);return;}
    wlan=sceWlanGetSwitchState()==1;
    if(wlan!=wifi_last_wlan) {
        ra_probe_log_printf(&logger,"WIFI_SWITCH enabled=%u action=%s\n",(unsigned)wlan,wlan?"RETRY_ENABLED":"WAIT_FOR_SWITCH");
        wifi_last_wlan=wlan;if(!wlan)ui_network_error(8,0,RA_REASON_WIFI_OFF,0);
    }
    __atomic_store_n(&wifi_online,state==PSP_NET_APCTL_STATE_GOT_IP && !wifi_policy.reset,__ATOMIC_RELEASE);
    if(state!=wifi_previous){
        ra_probe_log_printf(&logger,"WIFI_STATE old=%d new=%d\n",wifi_previous,state);
        if(state==4){record("WIFI_RECONNECTED",0);ui_network_notice(9,0);}
        else if(wifi_previous==4)ui_network_notice(8,0);
        wifi_previous=state;
    }
    action=ra_reconnect_step(&wifi_policy,now,state,wlan,
        ra_mail_phase(&mailbox)==RA_MAIL_IDLE,control->stop);
    if(action==RA_WIFI_CONNECT) {
        rc=sceNetApctlConnect(wifi_profile);
        if(rc>=0)network.connected=1;
        if(wifi_attempts<UINT32_MAX)++wifi_attempts;
        ra_probe_log_printf(&logger,"WIFI_CONNECT result=0x%08X attempt=%u\n",(unsigned)rc,wifi_attempts);
        if(rc<0){ra_reconnect_failed(&wifi_policy,now);ui_network_error(8,(unsigned)(wifi_policy.next-now),RA_REASON_APCTL,rc);}
        else ui_network_notice(7,0);
    } else if(action==RA_WIFI_DISCONNECT) {
        __atomic_store_n(&wifi_online,0,__ATOMIC_RELEASE);
        rc=sceNetApctlDisconnect();
        record("WIFI_RETRY_DISCONNECT",rc);
        if(rc<0)wifi_policy.reset=1;
    }
    if(wifi_policy.next>now && wifi_policy.next!=old_next) {
        ra_probe_log_printf(&logger,"WIFI_RETRY_WAIT delay_ms=%u after_attempt=%u reason=connect_error_or_timeout\n",(unsigned)(wifi_policy.next-now),wifi_attempts);
        ui_network_notice(8,(unsigned)(wifi_policy.next-now));
    }
}
static int wifi_begin(int wifi)
{
    int state,rc;
    if(!control->request_allowed || !network.apctl)return -35322;
    if(sceUtilityCheckNetParam(wifi))return -35319;
    rc=sceNetApctlGetState(&state);if(rc<0)return rc;
    if(state!=PSP_NET_APCTL_STATE_DISCONNECTED)return -35320;
    rc=start_apctl_events();if(rc<0)return rc;
    wifi_profile=wifi;wifi_enabled=1;return 0;
}
#endif
