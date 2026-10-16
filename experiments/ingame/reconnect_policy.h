#ifndef RA_RECONNECT_POLICY_H
#define RA_RECONNECT_POLICY_H
#include <stdint.h>
#include "rc_error.h"
enum { RA_WIFI_WAIT, RA_WIFI_CONNECT, RA_WIFI_DISCONNECT };
typedef struct {
    uint64_t next, started;
    unsigned failures, connecting, reset;
} ra_reconnect_policy;
static inline unsigned ra_retry_delay(unsigned failures)
{ return failures>=6?60000u:1000u<<failures; }
static inline void ra_reconnect_failed(ra_reconnect_policy* p,uint64_t now)
{
    p->connecting=0;p->next=now+ra_retry_delay(p->failures);
    if(p->failures<6)++p->failures;
}
/* Caller owns APCTL. Never disconnect while HTTP still owns a socket. */
static inline int ra_reconnect_step(ra_reconnect_policy* p,uint64_t now,
    int state,int wlan,int io_idle,int stop)
{
    if(stop)return RA_WIFI_WAIT;
    if(p->reset || (p->connecting && now-p->started>=30000u)) {
        if(!io_idle)return RA_WIFI_WAIT;
        p->reset=0;ra_reconnect_failed(p,now);
        return state?RA_WIFI_DISCONNECT:RA_WIFI_WAIT;
    }
    if(state==4){p->connecting=0;p->failures=0;p->next=now;return RA_WIFI_WAIT;}
    if(!wlan || state || now<p->next)return RA_WIFI_WAIT;
    if(p->connecting)return RA_WIFI_WAIT;
    p->connecting=1;p->started=now;return RA_WIFI_CONNECT;
}
static inline int ra_startup_retry(int result,int http)
{
    if(http==-1)return 0; /* Explicit permanent transport/TLS failure. */
    if(result==RC_INVALID_CREDENTIALS || result==RC_EXPIRED_TOKEN || result==RC_ACCESS_DENIED ||
       result==RC_ABORTED || result==RC_OUT_OF_MEMORY)return 0;
    return result==RC_NO_RESPONSE || http==-2 || http==429 || http==502 ||
        http==503 || http==504 || (http>=521 && http<=525);
}
#endif
