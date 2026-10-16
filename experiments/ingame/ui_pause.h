#ifndef RA_UI_PAUSE_H
#define RA_UI_PAUSE_H
#include <stdint.h>
/* One worker requests/releases; the existing game callback acknowledges.
 * No rc_client/queue lock is held while the game waits. */
enum { RA_UI_RUNNING, RA_UI_PAUSE_REQUESTED, RA_UI_PAUSE_ACK };
typedef struct { unsigned phase, heartbeat; } ra_ui_pause;
static inline unsigned ra_ui_pause_phase(const ra_ui_pause* p)
{ return __atomic_load_n(&p->phase,__ATOMIC_ACQUIRE); }
static inline void ra_ui_pause_heartbeat(ra_ui_pause* p,unsigned now)
{ __atomic_store_n(&p->heartbeat,now,__ATOMIC_RELEASE); }
static inline void ra_ui_pause_request(ra_ui_pause* p,unsigned now)
{
    ra_ui_pause_heartbeat(p,now);
    __atomic_store_n(&p->phase,RA_UI_PAUSE_REQUESTED,__ATOMIC_RELEASE);
}
/* The owner may spend seconds parsing the official set before frames start.
 * Startup cards are allowed, but no game thread may be parked for a menu yet. */
static inline int ra_ui_pause_request_ready(ra_ui_pause* p,unsigned now,int catalog_ready)
{
    if(!catalog_ready)return 0;
    ra_ui_pause_request(p,now);return 1;
}
static inline int ra_ui_pause_ack(ra_ui_pause* p)
{
    unsigned expected=RA_UI_PAUSE_REQUESTED;
    return __atomic_compare_exchange_n(&p->phase,&expected,RA_UI_PAUSE_ACK,0,
                                       __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static inline void ra_ui_pause_release(ra_ui_pause* p)
{ __atomic_store_n(&p->phase,RA_UI_RUNNING,__ATOMIC_RELEASE); }
static inline int ra_ui_pause_alive(const ra_ui_pause* p,unsigned now)
{ return (uint32_t)(now-__atomic_load_n(&p->heartbeat,__ATOMIC_ACQUIRE))<2000000u; }
typedef struct {
    void* context;
    unsigned (*now)(void*);
    int (*stopped)(void*);
    void (*yield)(void*);
} ra_ui_pause_api;
/* Called outside all engine/queue ownership. -1: worker heartbeat expired;
 * 0: no pause; 1: released or stopped. No heap/catalog pointers are borrowed. */
static inline int ra_ui_pause_checkpoint(ra_ui_pause* p,const ra_ui_pause_api* api)
{
    if(ra_ui_pause_phase(p)==RA_UI_RUNNING)return 0;
    (void)ra_ui_pause_ack(p);
    while(ra_ui_pause_phase(p)==RA_UI_PAUSE_ACK && !api->stopped(api->context)) {
        if(!ra_ui_pause_alive(p,api->now(api->context)))return -1;
        api->yield(api->context);
    }
    return 1;
}
#endif
