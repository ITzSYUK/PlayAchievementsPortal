#include "reconnect_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void policy_test(void)
{
    ra_reconnect_policy p={0};unsigned i;
    assert(ra_reconnect_step(&p,0,0,0,1,0)==RA_WIFI_WAIT);
    assert(ra_reconnect_step(&p,0,0,1,1,1)==RA_WIFI_WAIT && !p.connecting);
    assert(ra_reconnect_step(&p,0,0,1,1,0)==RA_WIFI_CONNECT);
    assert(ra_reconnect_step(&p,250,0,1,1,0)==RA_WIFI_WAIT && p.connecting);
    assert(ra_reconnect_step(&p,29999,2,1,1,0)==RA_WIFI_WAIT);
    assert(ra_reconnect_step(&p,30000,2,1,0,0)==RA_WIFI_WAIT && p.connecting);
    assert(ra_reconnect_step(&p,30001,2,1,1,0)==RA_WIFI_DISCONNECT && p.next==31001);
    assert(ra_reconnect_step(&p,31000,0,1,1,0)==RA_WIFI_WAIT);
    assert(ra_reconnect_step(&p,31001,0,1,1,0)==RA_WIFI_CONNECT);
    p.reset=1;
    assert(ra_reconnect_step(&p,31002,0,1,1,0)==RA_WIFI_WAIT && p.next==33002);
    assert(ra_reconnect_step(&p,33002,0,1,1,0)==RA_WIFI_CONNECT);
    assert(ra_reconnect_step(&p,33003,4,1,1,0)==RA_WIFI_WAIT && !p.failures && !p.connecting);
    assert(ra_reconnect_step(&p,33004,0,1,1,0)==RA_WIFI_CONNECT);
    for(i=0;i<1000;++i)ra_reconnect_failed(&p,40000+i);
    assert(p.failures==6 && p.next==100999);
    assert(!ra_startup_retry(RC_EXPIRED_TOKEN,503));
    assert(!ra_startup_retry(RC_INVALID_CREDENTIALS,-2));
    assert(!ra_startup_retry(RC_NO_RESPONSE,-1));
    assert(!ra_startup_retry(RC_OUT_OF_MEMORY,503));
    assert(!ra_startup_retry(RC_INVALID_JSON,200));
    assert(ra_startup_retry(RC_NO_RESPONSE,-2));
    assert(ra_startup_retry(RC_INVALID_JSON,503));
}
/* Compile the production PSP service with a fake native API. */
typedef struct { unsigned stop,request_allowed; } test_control;
static test_control shared={0,1},*control=&shared;
static struct { int apctl,connected; } network={1,0};
static struct { int phase; } mailbox;
static unsigned wifi_online;
static struct { int error; } logger;
static uint64_t time_ms;
static unsigned connects,disconnects,notices,logs;
static int native_state,native_result,wlan=1;
#define RA_MAIL_IDLE 0
#define PSP_NET_APCTL_STATE_DISCONNECTED 0
#define PSP_NET_APCTL_STATE_GOT_IP 4
static uint64_t now_ms(void* unused){(void)unused;return time_ms;}
static int ra_mail_phase(void* unused){(void)unused;return mailbox.phase;}
static int sceNetApctlGetState(int* state){*state=native_state;return 0;}
static int sceNetApctlConnect(int profile){assert(profile==2);++connects;return native_result;}
static int sceNetApctlDisconnect(void){++disconnects;native_state=0;return 0;}
static int sceWlanGetSwitchState(void){return wlan;}
static int sceUtilityCheckNetParam(int profile){assert(profile==2);return 0;}
static int start_apctl_events(void){return 0;}
static int record(const char* label,int rc){assert(label);++logs;return rc;}
static void ra_probe_log_printf(void* unused,const char* format,...){(void)unused;assert(format);++logs;}
static void ui_network_notice(unsigned type,unsigned delay){assert(type>=7 && type<=9);assert(delay<=60000);++notices;}
static void ui_network_error(unsigned type,unsigned delay,unsigned reason,int code)
{ assert(type==8 && reason);(void)code;ui_network_notice(type,delay); }
#include "../psp/ingame/https/user/reconnect.h"
int main(void)
{
    policy_test();(void)logger;
    assert(!wifi_begin(2));
    native_result=-5;wifi_tick();assert(connects==1 && notices && logs && !wifi_online);
    time_ms=500;wifi_tick();assert(connects==1);
    time_ms=1000;native_result=0;wifi_tick();assert(connects==2 && network.connected);
    time_ms=1250;native_state=4;wifi_tick();assert(wifi_online);
    time_ms=1500;wifi_policy.reset=1;mailbox.phase=1;wifi_tick();assert(!wifi_online && !disconnects);
    time_ms=1750;mailbox.phase=0;wifi_tick();assert(disconnects==1);
    time_ms=2000;wifi_tick();assert(connects==2);
    time_ms=2750;wifi_tick();assert(connects==3);
    time_ms=3000;control->stop=1;wifi_policy.reset=1;wifi_tick();assert(disconnects==1 && connects==3);
    puts("PASS reconnect: WLAN off, timeout/backoff, idle HTTP before disconnect, cancel, native failure/recovery, status notices/logs, permanent auth/TLS errors");
    return 0;
}
