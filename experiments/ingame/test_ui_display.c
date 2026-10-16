#include "ui_display.h"
#include "ui_pause.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
static ra_ui_pause parked;
static unsigned clock_us,stopped,finished;
static int checkpoint_result;
static unsigned clock_now(void* unused){(void)unused;return __atomic_load_n(&clock_us,__ATOMIC_ACQUIRE);}
static int is_stopped(void* unused){(void)unused;return (int)__atomic_load_n(&stopped,__ATOMIC_ACQUIRE);}
static void yield(void* unused){(void)unused;sched_yield();}
static void* game_callback(void* unused)
{
    const ra_ui_pause_api api={NULL,clock_now,is_stopped,yield};(void)unused;
    checkpoint_result=ra_ui_pause_checkpoint(&parked,&api);
    __atomic_store_n(&finished,1,__ATOMIC_RELEASE);return NULL;
}
static void pause_lifecycle(void)
{
    unsigned scenario,i;pthread_t game_thread;
    for(scenario=0;scenario<3;++scenario) {
        clock_us=stopped=finished=0;ra_ui_pause_request(&parked,0);
        assert(!pthread_create(&game_thread,NULL,game_callback,NULL));
        while(ra_ui_pause_phase(&parked)!=RA_UI_PAUSE_ACK)sched_yield();
        /* The worker remains free to idle/process responses while the game
         * callback is parked. No frames may execute beyond the checkpoint. */
        for(i=0;i<1000;++i)assert(!__atomic_load_n(&finished,__ATOMIC_ACQUIRE));
        if(scenario==0)ra_ui_pause_release(&parked);
        else if(scenario==1)__atomic_store_n(&stopped,1,__ATOMIC_RELEASE);
        else __atomic_store_n(&clock_us,2000000,__ATOMIC_RELEASE);
        assert(!pthread_join(game_thread,NULL));
        assert(checkpoint_result==(scenario==2?-1:1));ra_ui_pause_release(&parked);
    }
}
static void startup_menu_lifecycle(void)
{
    const ra_ui_pause_api api={NULL,clock_now,is_stopped,yield};
    unsigned attempt;pthread_t thread;
    memset(&parked,0,sizeof(parked));clock_us=stopped=finished=0;
    /* Reproduce 0.40: a startup menu parks the game while set parsing keeps
     * its worker busy for >2s. The retained watchdog correctly fires. */
    ra_ui_pause_request(&parked,0);clock_us=3000000;
    assert(ra_ui_pause_checkpoint(&parked,&api)==-1);ra_ui_pause_release(&parked);
    /* Fixed path: repeated hotkeys during login/load cannot park a game
     * callback. Long parsing therefore cannot expire a menu lease. */
    for(attempt=0;attempt<5;++attempt) {
        assert(!ra_ui_pause_request_ready(&parked,clock_us,0));
        clock_us+=3000000;
        assert(ra_ui_pause_checkpoint(&parked,&api)==0);
        assert(ra_ui_pause_phase(&parked)==RA_UI_RUNNING);
    }
    /* Once the server catalog is ready, repeated open/close still works. */
    for(attempt=0;attempt<3;++attempt) {
        assert(ra_ui_pause_request_ready(&parked,clock_us,1));
        assert(!pthread_create(&thread,NULL,game_callback,NULL));
        while(ra_ui_pause_phase(&parked)!=RA_UI_PAUSE_ACK)sched_yield();
        ra_ui_pause_release(&parked);assert(!pthread_join(thread,NULL));
        assert(checkpoint_result==1);
    }
    puts("PASS startup menu: slow set load reproduces old watchdog; pre-catalog hotkeys never pause; ready catalog allows repeated open/close");
}
typedef struct { ra_ui_fb current,next; int fail_get,fail_set,sets; } display_mock;
static int get(void* context,ra_ui_fb* out,int next)
{ display_mock* m=context;if(m->fail_get)return -1;*out=next?m->next:m->current;return 0; }
static int set(void* context,const ra_ui_fb* fb)
{ display_mock* m=context;++m->sets;if(m->fail_set)return -1;m->next=*fb;return 0; }
static void flip(display_mock* m){m->current=m->next;}
static const ra_ui_fb game={(void*)0x04000000,512,3};
static const ra_ui_fb foreign={(void*)0x04100000,512,3};
static void prepare(ra_ui_display* d,display_mock* m)
{
    memset(d,0,sizeof(*d));memset(m,0,sizeof(*m));m->current=m->next=game;
    d->buffer[0]=(ra_ui_fb){(void*)0x09a00000,512,0};
    d->buffer[1]=(ra_ui_fb){(void*)0x09b00000,512,0};
}
int main(void)
{
    ra_ui_display d;display_mock m;ra_ui_display_api api={&m,get,set};unsigned i;
    ra_ui_pause p={0,0};
    pause_lifecycle();
    startup_menu_lifecycle();
    assert(!ra_ui_pause_phase(&p));ra_ui_pause_request(&p,UINT32_MAX-10u);
    assert(ra_ui_pause_ack(&p) && !ra_ui_pause_ack(&p));
    assert(ra_ui_pause_alive(&p,10));assert(!ra_ui_pause_alive(&p,2000000));
    ra_ui_pause_release(&p);assert(!ra_ui_pause_ack(&p));
    for(i=0;i<100;++i) {
        prepare(&d,&m);assert(ra_ui_display_begin(&d,&api)==1);
        assert(ra_ui_display_refresh(&d,&api)==0);
        assert(ra_ui_display_back(&d)==0);
        assert(ra_ui_display_present(&d,&api,0)==0);
        assert(ra_ui_display_back(&d)==-1); /* queued buffer cannot be redrawn */
        assert(ra_ui_display_present(&d,&api,1)==-1);
        assert(ra_ui_display_refresh(&d,&api)==0 && d.front==-1);
        flip(&m);assert(ra_ui_display_refresh(&d,&api)==0 && d.front==0);
        assert(ra_ui_display_back(&d)==1);
        assert(ra_ui_display_present(&d,&api,0)==-1); /* displayed buffer protected */
        assert(ra_ui_display_present(&d,&api,1)==0);
        assert(ra_ui_display_end(&d,&api)==0 && d.active);
        assert(ra_ui_fb_equal(&m.next,&game));
        flip(&m);assert(ra_ui_display_end(&d,&api)==1 && !d.active);
    }
    prepare(&d,&m);m.next=foreign;assert(ra_ui_display_begin(&d,&api)==0);
    {
        ra_ui_fb interior={(void*)0x49a00010,480,3};
        assert(ra_ui_display_owns(&d,&interior));
        interior.pixels=(void*)0x09a00000;
        assert(ra_ui_display_owns(&d,&interior));
        interior.pixels=(void*)(uintptr_t)(0x09a00000u+512u*272u*2u);
        assert(!ra_ui_display_owns(&d,&interior));
    }
    m.fail_get=1;assert(ra_ui_display_begin(&d,&api)==-1);
    prepare(&d,&m);assert(ra_ui_display_begin(&d,&api)==1);
    m.fail_set=1;assert(ra_ui_display_present(&d,&api,0)==-1);
    m.fail_set=0;assert(ra_ui_display_present(&d,&api,0)==0);
    m.fail_get=1;assert(ra_ui_display_end(&d,&api)==-1 && d.active);
    m.fail_get=0;flip(&m);m.fail_set=1;
    assert(ra_ui_display_end(&d,&api)==-1 && d.active); /* retain allocation */
    m.fail_set=0;m.next=foreign;assert(ra_ui_display_end(&d,&api)==0);
    flip(&m);assert(ra_ui_display_end(&d,&api)==1 && ra_ui_fb_equal(&m.current,&foreign));
    prepare(&d,&m);assert(ra_ui_display_begin(&d,&api)==1);
    assert(ra_ui_display_present(&d,&api,0)==0);m.current=foreign;
    assert(ra_ui_display_refresh(&d,&api)==-1);
    assert(ra_ui_display_end(&d,&api)==0 && ra_ui_fb_equal(&m.next,&foreign));
    flip(&m);assert(ra_ui_display_end(&d,&api)==1);
    puts("PASS menu display: delayed flips, displayed/queued buffers protected, repeated open/close, GPU/display handoff errors retain memory, foreign HOME transition preserved, pause ack/cancel/watchdog wrap");
    return 0;
}
