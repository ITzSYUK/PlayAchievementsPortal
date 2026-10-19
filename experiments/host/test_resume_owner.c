#include "ra_client_session.h"
#include "ra_game_memory.h"
#include "ra_https.h"
#include "../ingame/ui_pause.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define PSP_POWER_CB_SUSPENDING 0x00010000
#define PSP_POWER_CB_POWER_SWITCH 0x80000000u
#define PSP_POWER_CB_RESUMING 0x00020000
#define PSP_POWER_CB_RESUME_COMPLETE 0x00040000
#define RA_MAIL_IDLE 0
static struct { unsigned stop,probe_installed,power_flags,resume_stage; int result; ra_game_map game_map; } ctl,*control=&ctl;
static struct { unsigned active; } ui_display;
static struct { unsigned reset; } wifi_policy;
static ra_ui_pause ui_pause;
static ra_game_reader game_reader;
static ra_client_session* runtime;
static unsigned sleep_events,sleep_epoch,sleep_was_active,recovery_busy,power_stop_flags;
static unsigned sleep_flags,delay_calls,wake_on_delay;
static unsigned engine_enabled,ui_tile_enabled,engine_last_us,ui_menu,ui_render_valid;
static unsigned report_ready,wifi_poll_at,wifi_online,mailbox,logger;
static unsigned resume_code_ready,reads,hashes,resets,stops,notices,errors;
static int read_ok,hash_result,resume_ok,display_closes,suspend_during_hash;
static uint64_t clock_ms,sleep_started,deadline,resume_after;
static unsigned char resume_code[64],memory_code[64];
static uint64_t now_ms(void* p) { (void)p;return clock_ms; }
static unsigned sceKernelGetSystemTimeLow(void) { return (unsigned)(clock_ms*1000); }
static unsigned ra_mail_phase(const unsigned* p) { return *p; }
static void engine_stop(void) { ++stops;engine_enabled=0; }
static void ui_present_menu(uint64_t now) { (void)now;if(display_closes)ui_display.active=0; }
static void ui_network_notice(unsigned type,unsigned delay) { (void)delay;notices=type; }
static void ui_network_error(unsigned type,unsigned delay,unsigned reason,int code)
{ (void)type;(void)delay;assert(reason==RA_REASON_RESUME && code<0);++errors; }
static unsigned read_game_segment(unsigned address,unsigned char* out,unsigned size,void* p)
{ (void)p;assert(address==0x08842D90 && size==64);++reads;memcpy(out,memory_code,size);return read_ok?size:0; }
static int power_event(int,int,void*);
static void resume_service(void);
static int cancelled(void* p)
{
    (void)p;
    if((sleep_events&2) && clock_ms>=resume_after && !recovery_busy)resume_service();
    return ctl.stop!=0;
}
static void sceKernelDelayThread(unsigned us)
{
    clock_ms+=us/1000;++delay_calls;
    if(wake_on_delay && delay_calls==wake_on_delay)power_event(0,PSP_POWER_CB_RESUMING,NULL);
}
static int disc_hash(char hash[33])
{
    ++hashes;memset(hash,0,33);clock_ms+=10;
    if(suspend_during_hash){suspend_during_hash=0;power_event(0,PSP_POWER_CB_SUSPENDING,NULL);}
    return hash_result;
}
static void ra_session_get_stats_mock(ra_client_session* s,ra_session_stats* out)
{ (void)s;memset(out,0,sizeof(*out));out->game_loaded=1; }
static int ra_session_resume_mock(ra_client_session* s) { (void)s;++resets;return resume_ok; }
static void ra_probe_log_printf(unsigned* log,const char* format,...)
{ (void)log;(void)format; }
#define ra_session_get_stats ra_session_get_stats_mock
#define ra_session_resume ra_session_resume_mock
#include "../psp/ingame/https/user/resume_owner.h"
static void setup(void)
{
    memset(&ctl,0,sizeof(ctl));memset(&game_reader,0,sizeof(game_reader));
    game_reader.map.count=1;game_reader.map.regions[0]=(ra_game_region){0x08804000,0x100000};
    ctl.game_map=game_reader.map;ctl.probe_installed=1;
    sleep_events=sleep_epoch=report_ready=mailbox=power_stop_flags=0;
    delay_calls=wake_on_delay=0;sleep_flags=0;resume_after=0;
    engine_enabled=ui_tile_enabled=resume_code_ready=1;
    reads=hashes=resets=stops=notices=errors=recovery_busy=0;
    read_ok=resume_ok=display_closes=1;hash_result=suspend_during_hash=0;
    ui_display.active=0;clock_ms=100;deadline=500;ui_menu=1;
    memset(resume_code,0x42,64);memcpy(memory_code,resume_code,64);
    memset(&ui_pause,0,sizeof(ui_pause));
}
static void sleep_and_wake(void)
{
    assert(!power_event(0,PSP_POWER_CB_SUSPENDING,NULL));
    assert(!engine_enabled && !ui_tile_enabled && sleep_events==1 && sleep_was_active);
    assert(!reads && !hashes && !resets && !stops && !ctl.stop); /* callback never waits or reads */
    clock_ms=200;resume_service();assert(!reads && !hashes && !resets);
    assert(!power_event(0,PSP_POWER_CB_RESUMING,NULL));assert(sleep_events==3);
    /* Deliberately no COMPLETE: real SDK may defer it until another event. */
}
int main(void)
{
    unsigned mode;
    setup();sleep_and_wake();
    mailbox=1;resume_service();assert(!reads && !stops); /* active HTTP must cancel/drain first */
    mailbox=0;report_ready=1;resume_service();assert(!reads && !stops);
    report_ready=0;ui_display.active=1;display_closes=0;resume_service();assert(!reads && !recovery_busy);
    display_closes=1;resume_service();
    assert(reads==1 && hashes==1 && resets==1 && !ctl.stop && !sleep_events && engine_enabled);
    assert(!ui_menu && notices==16 && wifi_policy.reset && !wifi_online && deadline==610);
    assert(engine_last_us==210000 && !recovery_busy);
    for(mode=0;mode<7;++mode) {
        setup();sleep_and_wake();
        if(mode==0)ctl.game_map.count=0;
        if(mode==1)ctl.probe_installed=0;
        if(mode==2)read_ok=0;
        if(mode==3)memory_code[0]^=1;
        if(mode==4)hash_result=-7;
        if(mode==5)resume_ok=0;
        if(mode==6)game_reader.map.regions[0].size=0;
        resume_service();assert(ctl.stop && ctl.result<0 && !engine_enabled && errors==1 && !recovery_busy);
        if(mode!=5)assert(!resets);
    }
    setup();engine_enabled=0;resume_code_ready=0;
    power_event(0,PSP_POWER_CB_SUSPENDING,NULL);clock_ms=200;
    power_event(0,PSP_POWER_CB_RESUME_COMPLETE,NULL);resume_service();
    assert(!reads && !resets && !engine_enabled && !ctl.stop && !sleep_events);
    setup();sleep_and_wake();ctl.stop=1;resume_service();assert(!reads && !stops);
    setup();deadline=UINT64_MAX-1;sleep_and_wake();resume_service();assert(deadline==UINT64_MAX);
    setup();sleep_and_wake();suspend_during_hash=1;resume_service();
    assert(sleep_events==1 && !engine_enabled && !ctl.stop && deadline==500);
    power_event(0,PSP_POWER_CB_RESUME_COMPLETE,NULL);resume_service();
    assert(!sleep_events && engine_enabled && hashes==2 && deadline==620);
    setup();power_event(0,PSP_POWER_CB_POWER_SWITCH|PSP_POWER_CB_RESUME_COMPLETE,NULL);
    assert(sleep_events==1); /* sticky COMPLETE cannot undo a fresh suspend */
    power_event(0,PSP_POWER_CB_RESUMING|PSP_POWER_CB_POWER_SWITCH,NULL);resume_service();
    assert(!sleep_events && engine_enabled && !ctl.stop);
    setup();power_event(0,PSP_POWER_CB_POWER_SWITCH,NULL);wake_on_delay=3;
    assert(check()==0 && !sleep_events && engine_enabled && delay_calls>=16);
    assert(hashes==1 && !ctl.stop); /* startup/observation caller was parked */
    power_event(0,PSP_POWER_CB_RESUME_COMPLETE,NULL);assert(!sleep_events && engine_enabled);
    resume_service();assert(hashes==1); /* late COMPLETE cannot restart recovery */
    setup();recovery_busy=1;sleep_events=1;
    assert(check()==RA_HTTPS_CANCEL && !ctl.stop); /* abort hash if sleep repeats */
    setup();power_event(0,0x40000000,NULL);assert(!sleep_events && engine_enabled); /* HOLD */
    power_event(0,PSP_POWER_CB_RESUMING,NULL);resume_service(); /* coalesced notifications */
    assert(!sleep_events && engine_enabled && !ctl.stop);
    puts("PASS power recovery: callback disables frames without I/O, waits for transport/report/display, verifies map/code/hash/memory, rejects seven failures, startup and deadline overflow");
    return 0;
}
