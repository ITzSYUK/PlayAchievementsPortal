#ifndef RA_ONLINE_UI_H
#define RA_ONLINE_UI_H
/* Worker prepares tiles, menu, input, logs and audio. The display import
 * wrapper only copies an immutable tile before the game's presentation. */
#include <pspaudio.h>
#include <pspge.h>
#include "ra_ui.h"
#include "ui_pause.h"
#include "ui_display.h"
static ra_ui* online_ui;
static int ui_ready, ui_menu, ui_combo_latched;
static unsigned ui_previous, ui_filter, ui_selected;
static uint64_t ui_hold_started, ui_notice_until;
static ra_ui_notice ui_notice;
static unsigned ui_network_pending,ui_network_delay;
static unsigned ui_home_seen;
/* Worker-only status path, separate from the callback's SPSC achievement queue. */
static void ui_network_notice(unsigned type,unsigned delay_ms)
{ ui_network_pending=type;ui_network_delay=(delay_ms+999)/1000; }
static int ui_audio_channel=-1;
static short* ui_pcm;
static unsigned ui_surface_failures;
static ra_ui_pause ui_pause;
static uint16_t* ui_tile;
static unsigned ui_tile_gate,ui_tile_enabled,ui_tile_deadline,ui_notice_serial=1,ui_tile_serial;
static unsigned ui_notice_calls,ui_notice_drawn,ui_notice_skipped,ui_notice_max_us;
static int ui_separate_buffer(uint32_t address,unsigned bytes,void* other,int stride,int format)
{
    uint32_t base=(uint32_t)(uintptr_t)other&~0x40000000u;
    unsigned size;
    ra_ui_surface s={other,(unsigned)stride,(unsigned)format};
    if(!other)return 1;
    if(!ra_ui_surface_valid(&s))return 0;
    size=(unsigned)stride*272u*(format==3?4u:2u);
    address&=~0x40000000u;
    if(base>UINT32_MAX-size || address>UINT32_MAX-bytes)return 0;
    return address>=base+size || base>=address+bytes;
}
static void ui_notice_status(void)
{
    ra_probe_log_printf(&logger,"NOTICE_STATUS calls=%u drawn=%u skipped=%u max_us=%u\n",
        __atomic_load_n(&ui_notice_calls,__ATOMIC_RELAXED),__atomic_load_n(&ui_notice_drawn,__ATOMIC_RELAXED),
        __atomic_load_n(&ui_notice_skipped,__ATOMIC_RELAXED),__atomic_load_n(&ui_notice_max_us,__ATOMIC_RELAXED));
}
/* Only the game's import reaches this wrapper; the plugin keeps its own
 * original SDK import. The kernel never redirects the global syscall. */
int ra_notice_present(void* pixels,int stride,int format,int sync)
{
    ra_client_control* shared=control;
    if(shared) {
        __sync_add_and_fetch(&shared->notice_active,1);
        __atomic_add_fetch(&ui_notice_calls,1,__ATOMIC_RELAXED);
        if(!shared->stop && __atomic_load_n(&ui_tile_enabled,__ATOMIC_ACQUIRE) &&
           !__sync_lock_test_and_set(&ui_tile_gate,1)) {
            uint32_t start=sceKernelGetSystemTimeLow(),address=(uint32_t)(uintptr_t)pixels;
            unsigned bytes;void* current=NULL;void* next=NULL;int current_stride,current_format;
            ra_ui_surface surface={pixels,(unsigned)stride,(unsigned)format};
            bytes=(unsigned)stride*272u*(format==3?4u:2u);
            if(!shared->stop && __atomic_load_n(&ui_tile_enabled,__ATOMIC_ACQUIRE) &&
               (int32_t)(start-ui_tile_deadline)<0 && ra_ui_pause_phase(&ui_pause)==RA_UI_RUNNING &&
               !(address&0x80000000u) && ra_ui_surface_valid(&surface) &&
               (address&((format==3)?3u:1u))==0 && sceGeDrawSync(1)==PSP_GE_LIST_DONE &&
               sceDisplayGetFrameBuf(&current,&current_stride,&current_format,PSP_DISPLAY_SETBUF_IMMEDIATE)>=0 &&
               ui_separate_buffer(address,bytes,current,current_stride,current_format) &&
               sceDisplayGetFrameBuf(&next,&current_stride,&current_format,PSP_DISPLAY_SETBUF_NEXTFRAME)>=0 &&
               ui_separate_buffer(address,bytes,next,current_stride,current_format)) {
                address&=~0x40000000u;
                if((address>=0x04000000u && address<0x04200000u && bytes<=0x04200000u-address) ||
                   ra_client_user_range(address,bytes)) {
                    surface.pixels=(void*)(uintptr_t)(address|0x40000000u);
                    ra_ui_blit_notice(ui_tile,&surface);
                    __atomic_add_fetch(&ui_notice_drawn,1,__ATOMIC_RELAXED);
                } else __atomic_add_fetch(&ui_notice_skipped,1,__ATOMIC_RELAXED);
            } else __atomic_add_fetch(&ui_notice_skipped,1,__ATOMIC_RELAXED);
            if(sceKernelGetSystemTimeLow()-start>ui_notice_max_us)
                __atomic_store_n(&ui_notice_max_us,sceKernelGetSystemTimeLow()-start,__ATOMIC_RELAXED);
            __sync_lock_release(&ui_tile_gate);
        }
    }
    {
        int result=sceDisplaySetFrameBuf(pixels,stride,format,sync);
        if(shared)__sync_sub_and_fetch(&shared->notice_active,1);
        return result;
    }
}
static ra_ui_display ui_display;
static uint64_t ui_transition_ms;
static unsigned ui_render_hash;
static int ui_render_valid;
#define UI_BUFFER_BYTES (512u*272u*2u)
static int ui_framebuffer(ra_ui_surface* surface);
static int ui_get_display(void* unused,ra_ui_fb* fb,int next)
{
    int rc;(void)unused;
    rc=sceDisplayGetFrameBuf(&fb->pixels,&fb->stride,&fb->format,next?PSP_DISPLAY_SETBUF_NEXTFRAME:PSP_DISPLAY_SETBUF_IMMEDIATE);
    if(rc<0)ra_probe_log_printf(&logger,"UI_DISPLAY_GET next=%d error=0x%08X\n",next,(unsigned)rc);
    return rc;
}
static int ui_set_display(void* unused,const ra_ui_fb* fb)
{
    int rc;(void)unused;
    rc=sceDisplaySetFrameBuf(fb->pixels,fb->stride,fb->format,PSP_DISPLAY_SETBUF_NEXTFRAME);
    if(rc<0)ra_probe_log_printf(&logger,"UI_DISPLAY_SET address=0x%08X stride=%d format=%d error=0x%08X\n",
        (unsigned)(uintptr_t)fb->pixels,fb->stride,fb->format,(unsigned)rc);
    return rc;
}
static const ra_ui_display_api ui_display_api={NULL,ui_get_display,ui_set_display};
static void ui_release_pause(void)
{
    /* Worker is outside engine_gate here. Give the resumed game callback
     * ownership before stale-frame idle detection is eligible again. */
    engine_last_us=sceKernelGetSystemTimeLow();
    ra_ui_pause_release(&ui_pause);ui_render_valid=0;
}
static unsigned ui_pause_clock(void* unused)
{ (void)unused;return sceKernelGetSystemTimeLow(); }
static int ui_pause_stopped(void* unused)
{ (void)unused;return control->stop!=0; }
static void ui_pause_yield(void* unused)
{ (void)unused;sceKernelDelayThread(1000); }
static const ra_ui_pause_api ui_pause_api={NULL,ui_pause_clock,ui_pause_stopped,ui_pause_yield};
static void ui_pause_point(void)
{
    if(ra_ui_pause_checkpoint(&ui_pause,&ui_pause_api)<0) {
        control->result=-35420;control->stop=1;
    }
    engine_last_us=sceKernelGetSystemTimeLow();
}
static unsigned ui_fingerprint(void)
{
    unsigned i,hash=ui_filter*131u+ui_selected;
    for(i=0;i<online_ui->count;++i)hash=hash*16777619u+ra_ui_state(online_ui,i);
    return hash;
}
static void ui_present_menu(uint64_t now)
{
    int rc,back;unsigned hash;
    ra_ui_surface surface;
    ra_ui_pause_heartbeat(&ui_pause,sceKernelGetSystemTimeLow());
    if(!ui_menu) {
        rc=ra_ui_display_end(&ui_display,&ui_display_api);
        if(rc==1 && ra_ui_pause_phase(&ui_pause)!=RA_UI_RUNNING) {
            unsigned menu_buttons=PSP_CTRL_CIRCLE|PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER|
                PSP_CTRL_SELECT|PSP_CTRL_TRIANGLE|PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT;
            if(ui_previous&menu_buttons)return; /* no close/navigation key leaks into game */
            ui_release_pause();ra_probe_log_printf(&logger,"UI_CLOSE restored=1\n");
        }
        if(rc<0){control->result=-35419;control->stop=1;}
        return;
    }
    if(ra_ui_pause_phase(&ui_pause)==RA_UI_RUNNING) {
        if(!ra_ui_pause_request_ready(&ui_pause,sceKernelGetSystemTimeLow(),online_ui->count!=0)) {
            ui_menu=0;ui_notice_until=now;ui_network_notice(14,0);
            ra_probe_log_printf(&logger,"UI_WAIT_CATALOG rows=0 game_paused=0\n");return;
        }
        ui_transition_ms=now;
        ra_probe_log_printf(&logger,"UI_PAUSE_REQUEST\n");return;
    }
    if(!ui_display.active) {
        if(now-ui_transition_ms>1500) {
            ra_probe_log_printf(&logger,"UI_OPEN_ABORT phase=%u ge=%d\n",ra_ui_pause_phase(&ui_pause),sceGeDrawSync(1));
            ui_menu=0;ui_release_pause();return;
        }
        if(ra_ui_pause_phase(&ui_pause)!=RA_UI_PAUSE_ACK || sceGeDrawSync(1)!=PSP_GE_LIST_DONE)return;
        if(sceDisplayWaitVblankStart()<0 || !ui_framebuffer(&surface)) {
            ++ui_surface_failures;ui_menu=0;ui_release_pause();return;
        }
        rc=ra_ui_display_begin(&ui_display,&ui_display_api);
        if(rc<0){++ui_surface_failures;ui_menu=0;ui_release_pause();return;}
        if(!rc)return;
        {
            ra_ui_fb validated={surface.pixels,(int)surface.stride,(int)surface.format};
            if(!ra_ui_fb_equal(&validated,&ui_display.saved)){ui_menu=0;return;}
        }
        ra_probe_log_printf(&logger,"UI_OPEN saved=0x%08X stride=%d format=%d own_format=565\n",
            (unsigned)(uintptr_t)ui_display.saved.pixels,ui_display.saved.stride,ui_display.saved.format);
    }
    rc=ra_ui_display_refresh(&ui_display,&ui_display_api);
    if(rc<0 || (ui_display.pending>=0 && now-ui_transition_ms>500)) {
        ra_probe_log_printf(&logger,"UI_DISPLAY_CONFLICT result=%d pending=%d\n",rc,ui_display.pending);
        ui_menu=0;return;
    }
    back=ra_ui_display_back(&ui_display);hash=ui_fingerprint();
    if(back<0 || (ui_render_valid && hash==ui_render_hash))return;
    surface=(ra_ui_surface){ui_display.buffer[back].pixels,512,PSP_DISPLAY_PIXEL_FORMAT_565};
    memset(surface.pixels,0,UI_BUFFER_BYTES);
    ra_ui_draw(online_ui,&surface,1,ui_filter,ui_selected,NULL);
    sceKernelDcacheWritebackRange(surface.pixels,UI_BUFFER_BYTES);
    if(ra_ui_display_present(&ui_display,&ui_display_api,back)<0){ui_menu=0;return;}
    ui_render_hash=hash;ui_render_valid=1;ui_transition_ms=now;
}
static void ui_log_notice(const ra_ui_notice* notice)
{
    ra_probe_log_printf(&logger,"ACHIEVEMENT_EVENT id=%u state=%s spectator=%u time_ms=%u\n",
        notice->id,notice->type==RA_SESSION_EVENT_CONDITION?"CONDITION_MET":
        notice->type==RA_SESSION_EVENT_CONFIRMED?"SERVER_CONFIRMED":
        notice->type==RA_SESSION_EVENT_ENCORE?"ENCORE_REPLAY":
        notice->type==RA_SESSION_EVENT_REJECTED?"SERVER_REJECTED":"CLIENT_WARNING",
        online_ui->spectator,notice->time_ms);
}
static void ui_audio_tick(void)
{
    if(ui_audio_channel>=0 && sceAudioGetChannelRestLength(ui_audio_channel)==0) {
        if(sceAudioChRelease(ui_audio_channel)>=0)ui_audio_channel=-1;
    }
}
static void ui_sound(void)
{
    unsigned i,phase=0;
    ui_audio_tick();if(ui_audio_channel>=0 || !ui_pcm)return;
    for(i=0;i<4096;++i) {
        int value;unsigned envelope=i<128?i:4096-i<256?(4096-i)/2:128;
        phase+=(i<2048?660u:880u)*97392u;
        value=(int)((phase>>16)&65535u);
        value=value<32768?value*2-32768:98303-value*2;
        ui_pcm[i]=(short)(value*(int)envelope/512);
    }
    sceKernelDcacheWritebackRange(ui_pcm,4096*sizeof(*ui_pcm));
    ui_audio_channel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,4096,PSP_AUDIO_FORMAT_MONO);
    if(ui_audio_channel>=0 && sceAudioOutput(ui_audio_channel,PSP_AUDIO_VOLUME_MAX/2,ui_pcm)<0) {
        if(sceAudioChRelease(ui_audio_channel)>=0)ui_audio_channel=-1;
    }
}
static void ui_owner_events(void)
{
    ra_session_event event;
    if(!ui_ready)return;
    while(ra_session_take_event(runtime,&event))
        ra_ui_publish(online_ui,event.type,event.achievement_id,event.time_ms);
}
/* A validated PSP framebuffer can reside in EDRAM or partition 2. Accept
 * cached/uncached aliases, never kernel addresses or an overflowing extent. */
static int ui_framebuffer(ra_ui_surface* surface)
{
    void* pixels=NULL;int stride,format,mode,width,height;
    uint32_t address,bytes;
    if(sceDisplayGetMode(&mode,&width,&height)<0 || width!=480 || height!=272 ||
       sceDisplayGetFrameBuf(&pixels,&stride,&format,PSP_DISPLAY_SETBUF_IMMEDIATE)<0)return 0;
    surface->pixels=pixels;surface->stride=(unsigned)stride;surface->format=(unsigned)format;
    if(!ra_ui_surface_valid(surface))return 0;
    address=(uint32_t)(uintptr_t)pixels;
    if(address&0x80000000u)return 0;
    address&=~0x40000000u;
    bytes=surface->stride*272u*(format==3?4u:2u);
    if((address&(format==3?3u:1u)) || !(
       (address>=0x04000000u && address<0x04200000u && bytes<=0x04200000u-address) ||
       ra_client_user_range(address,bytes)))return 0;
    surface->pixels=(void*)(uintptr_t)(address|0x40000000u);return 1;
}
static void ui_poll(void)
{
    SceCtrlData pad;ra_ui_surface surface;
    unsigned buttons,pressed,count;uint64_t now=now_ms(NULL);
    if(!ui_ready)return;
    ui_audio_tick();
    if(sceCtrlPeekBufferPositive(&pad,1)>0) {
        unsigned combo=PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER|PSP_CTRL_SELECT;
        buttons=pad.Buttons;pressed=buttons&~ui_previous;ui_previous=buttons;
        if(pressed&PSP_CTRL_HOME){++ui_home_seen;ra_probe_log_printf(&logger,"HOME_BUTTON seen=%u exit=NOT_CONFIRMED\n",ui_home_seen);}
        if((buttons&combo)==combo) {
            if(!ui_hold_started)ui_hold_started=now;
            if(!ui_combo_latched && now-ui_hold_started>=400) {
                ui_menu=!ui_menu;ui_combo_latched=1;
            }
        } else {
            ui_hold_started=0;ui_combo_latched=0;
            if(buttons&PSP_CTRL_HOME || pressed&PSP_CTRL_CIRCLE)ui_menu=0;
            else if(ui_menu) {
                if(pressed&PSP_CTRL_TRIANGLE){ui_filter=(ui_filter+1)%3;ui_selected=0;}
                if(pressed&PSP_CTRL_UP && ui_selected)--ui_selected;
                if(pressed&PSP_CTRL_DOWN)++ui_selected;
                if(pressed&PSP_CTRL_LEFT)ui_selected=ui_selected>7?ui_selected-7:0;
                if(pressed&PSP_CTRL_RIGHT)ui_selected+=7;
            }
        }
    } else if(ui_menu || ra_ui_pause_phase(&ui_pause)!=RA_UI_RUNNING) {
        ui_menu=0;ui_previous=0;
        ra_probe_log_printf(&logger,"UI_INPUT_UNAVAILABLE close=1\n");
    }
    count=ra_ui_count(online_ui,ui_filter);
    if(ui_selected>=count)ui_selected=count?count-1:0;
    if(now>=ui_notice_until) {
        __atomic_store_n(&ui_tile_enabled,0,__ATOMIC_RELEASE);
        unsigned tries;
        if(ui_network_pending) {
            ui_notice=(ra_ui_notice){ui_network_pending,ui_network_delay,0};ui_network_pending=0;
            ui_notice_until=now+3500;++ui_notice_serial;
        } else for(tries=0;tries<RA_UI_NOTICES && ra_ui_take(online_ui,&ui_notice);++tries) {
            unsigned i;
            ui_log_notice(&ui_notice);
            for(i=0;i<online_ui->count;++i)if(online_ui->rows[i].id==ui_notice.id)break;
            /* Do not show stale PENDING after the acknowledgement arrived. */
            if(ui_notice.type==RA_SESSION_EVENT_CONDITION && i<online_ui->count &&
               ra_ui_state(online_ui,i)==RA_UI_CONFIRMED)continue;
            ui_notice_until=now+4000;
            ++ui_notice_serial;
            if(ui_notice.type==RA_SESSION_EVENT_CONFIRMED || ui_notice.type==RA_SESSION_EVENT_ENCORE ||
               (ui_notice.type==RA_SESSION_EVENT_CONDITION && online_ui->spectator))ui_sound();
            break;
        }
    }
    if(ui_menu || ui_display.active || ra_ui_pause_phase(&ui_pause)!=RA_UI_RUNNING) {
        __atomic_store_n(&ui_tile_enabled,0,__ATOMIC_RELEASE);
        ui_present_menu(now);return;
    }
    if(now>=ui_notice_until)return;
    if(!control->notice_installed)return; /* no unsafe worker fallback */
    if((ui_tile_serial!=ui_notice_serial || !__atomic_load_n(&ui_tile_enabled,__ATOMIC_ACQUIRE)) &&
       !__sync_lock_test_and_set(&ui_tile_gate,1)) {
        unsigned row;
        surface=(ra_ui_surface){ui_display.buffer[0].pixels,512,PSP_DISPLAY_PIXEL_FORMAT_565};
        ra_ui_draw(online_ui,&surface,0,0,0,&ui_notice);
        for(row=0;row<RA_NOTICE_HEIGHT;++row)
            memcpy(ui_tile+row*RA_NOTICE_WIDTH,(uint16_t*)surface.pixels+(row+RA_NOTICE_Y)*512+RA_NOTICE_X,RA_NOTICE_WIDTH*2);
        ui_tile_deadline=sceKernelGetSystemTimeLow()+(unsigned)(ui_notice_until-now)*1000u;
        ui_tile_serial=ui_notice_serial;
        __atomic_store_n(&ui_tile_enabled,1,__ATOMIC_RELEASE);
        __sync_lock_release(&ui_tile_gate);
    }
}
static int ui_prepare(void)
{
    unsigned i;
    online_ui=malloc(sizeof(*online_ui));
    if(!online_ui)return -35417;
    ra_ui_init(online_ui,!online_mode);online_ui->encore=encore_mode;
    for(i=0;i<2;++i) {
        ui_display.buffer[i]=(ra_ui_fb){memalign(64,UI_BUFFER_BYTES),512,PSP_DISPLAY_PIXEL_FORMAT_565};
        if(!ui_display.buffer[i].pixels)return -35417;
    }
    ui_tile=malloc(RA_NOTICE_WIDTH*RA_NOTICE_HEIGHT*2u);
    if(!ui_tile)return -35417;
    ui_pcm=memalign(64,4096*sizeof(*ui_pcm)); /* optional sound */
    ui_notice=(ra_ui_notice){7,0,0};ui_notice_until=now_ms(NULL)+2500;
    ui_ready=1;return 0;
}
static int ui_shutdown(void)
{
    unsigned tries;int rc=0;
    /* engine_stop precedes this. No callback may access catalog afterwards. */
    ui_ready=0;ui_menu=0;
    __atomic_store_n(&ui_tile_enabled,0,__ATOMIC_RELEASE);
    while(__sync_lock_test_and_set(&ui_tile_gate,1))sceKernelDelayThread(1000);
    free(ui_tile);ui_tile=NULL;
    __sync_lock_release(&ui_tile_gate);
    ui_notice_status();
    for(tries=0;tries<100;++tries) {
        rc=ra_ui_display_end(&ui_display,&ui_display_api);
        if(rc!=0)break;
        ra_ui_pause_heartbeat(&ui_pause,sceKernelGetSystemTimeLow());
        sceKernelDelayThread(5000);
    }
    ui_release_pause();
    if(rc!=1)return -35419; /* retain heap if scanout may still reference it */
    if(ui_audio_channel>=0) {
        unsigned tries;
        for(tries=0;tries<30 && sceAudioGetChannelRestLength(ui_audio_channel)>0;++tries)
            sceKernelDelayThread(10000);
        if(sceAudioChRelease(ui_audio_channel)<0)return -35418;
        ui_audio_channel=-1;
    }
    free(ui_display.buffer[0].pixels);free(ui_display.buffer[1].pixels);
    ui_display.buffer[0].pixels=ui_display.buffer[1].pixels=NULL;
    free(ui_pcm);ui_pcm=NULL;free(online_ui);online_ui=NULL;return 0;
}
#endif
