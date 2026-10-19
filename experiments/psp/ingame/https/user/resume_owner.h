#ifndef RA_RESUME_OWNER_H
#define RA_RESUME_OWNER_H
/* Worker recovery, shared with the mocked power/network/memory regression. */
static int power_event(int unused, int flags, void* context)
{
    (void)unused; (void)context;
    if ((unsigned)flags & (PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_RESUMING | PSP_POWER_CB_RESUME_COMPLETE)) {
        power_stop_flags=(unsigned)flags;
        if(!__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE)) {
            sleep_was_active=engine_enabled;sleep_started=now_ms(NULL);
        }
        __atomic_store_n(&engine_enabled,0,__ATOMIC_RELEASE);
        if((unsigned)flags&PSP_POWER_CB_RESUME_COMPLETE)
            __atomic_fetch_or(&sleep_events,3u,__ATOMIC_ACQ_REL);
        else if((unsigned)flags&(PSP_POWER_CB_SUSPENDING|PSP_POWER_CB_POWER_SWITCH)) {
            ++sleep_epoch;
            __atomic_store_n(&sleep_events,1u,__ATOMIC_RELEASE);
        } else __atomic_fetch_or(&sleep_events,1u,__ATOMIC_ACQ_REL);
        __atomic_store_n(&ui_tile_enabled,0,__ATOMIC_RELEASE);
        ra_ui_pause_release(&ui_pause);
    }
    return 0;
}
static void resume_service(void)
{
    unsigned flags=__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE);
    uint64_t old_deadline;char hash[33];unsigned char code[64];int rc=0;
    unsigned epoch=sleep_epoch;
    ra_session_stats stats;
    if(!(flags&2u) || control->stop || ra_mail_phase(&mailbox)!=RA_MAIL_IDLE ||
       __atomic_load_n(&report_ready,__ATOMIC_ACQUIRE))return;
    recovery_busy=1;engine_stop();ui_menu=0;
    if(ui_display.active) {
        ui_present_menu(now_ms(NULL));
        if(ui_display.active){recovery_busy=0;return;}
    }
    ui_network_notice(15,0);
    old_deadline=deadline;deadline=now_ms(NULL)+100000u;
    if(resume_code_ready) {
        if(!ra_game_map_valid(&game_reader.map) || memcmp(&control->game_map,&game_reader.map,sizeof(game_reader.map)) ||
           !control->probe_installed || read_game_segment(0x08842D90u,code,sizeof(code),NULL)!=sizeof(code) ||
           memcmp(code,resume_code,sizeof(code)))rc=-35433;
        if(!rc)rc=disc_hash(hash);
        ra_session_get_stats(runtime,&stats);
        if(!rc && stats.game_loaded && !stats.load_pending && !ra_session_resume(runtime))rc=-35433;
    } else if(sleep_was_active)rc=-35433;
    if(epoch!=sleep_epoch || !(__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE)&2u)) {
        deadline=old_deadline;recovery_busy=0;return;
    }
    if(old_deadline!=UINT64_MAX) {
        uint64_t gap=now_ms(NULL)-sleep_started;
        old_deadline=gap>UINT64_MAX-old_deadline?UINT64_MAX:old_deadline+gap;
    }
    deadline=old_deadline;
    if(rc) {
        ui_network_error(12,0,RA_REASON_RESUME,rc);
        ra_probe_log_printf(&logger,"RESUME_REJECT result=%d frames_disabled=1\n",rc);
        control->result=rc;control->stop=1;
    } else {
        wifi_policy.reset=1;wifi_poll_at=0;
        __atomic_store_n(&wifi_online,0,__ATOMIC_RELEASE);
        engine_last_us=sceKernelGetSystemTimeLow();
        __atomic_store_n(&sleep_events,0,__ATOMIC_RELEASE);
        __atomic_store_n(&engine_enabled,sleep_was_active,__ATOMIC_RELEASE);
        ui_render_valid=0;ui_network_notice(16,0);
        ra_probe_log_printf(&logger,"RESUME_OK memory=%s history=RESET_NO_CATCHUP pending=RETAINED\n",resume_code_ready?"VERIFIED":"STARTUP_VALIDATION_PENDING");
    }
    recovery_busy=0;
}
#endif
