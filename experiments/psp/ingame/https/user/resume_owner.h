#ifndef RA_RESUME_OWNER_H
#define RA_RESUME_OWNER_H
/* Worker recovery, shared with the mocked power/network/memory regression. */
static int power_event(int unused, int flags, void* context)
{
    (void)unused; (void)context;
    unsigned wake=((unsigned)flags&PSP_POWER_CB_RESUMING) && !((unsigned)flags&PSP_POWER_CB_SUSPENDING);
    unsigned suspend=(unsigned)flags&(PSP_POWER_CB_SUSPENDING|PSP_POWER_CB_POWER_SWITCH);
    unsigned active=__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE);
    if(wake || suspend || (active && ((unsigned)flags&PSP_POWER_CB_RESUME_COMPLETE))) {
        power_stop_flags=(unsigned)flags;
        if(!active) {
            sleep_was_active=engine_enabled;sleep_started=now_ms(NULL);
            sleep_flags=0;
            control->resume_stage=0;
        }
        sleep_flags|=(unsigned)flags;
        __atomic_store_n(&engine_enabled,0,__ATOMIC_RELEASE);
        if(wake || (!suspend && ((unsigned)flags&PSP_POWER_CB_RESUME_COMPLETE))) {
            /* PSPSDK: COMPLETE may be delayed until an unrelated power event.
             * RESUMING delivered to the running owner is sufficient to poll
             * recovery; allow storage a short settling interval first. */
            if(!(active&2u))resume_after=now_ms(NULL)+250u;
            __atomic_store_n(&sleep_events,3u,__ATOMIC_RELEASE);
        } else if(suspend) {
            ++sleep_epoch;
            __atomic_store_n(&sleep_events,1u,__ATOMIC_RELEASE);
        }
        control->power_flags=sleep_flags;
        __atomic_store_n(&ui_tile_enabled,0,__ATOMIC_RELEASE);
        ra_ui_pause_release(&ui_pause);
    }
    return 0;
}
static void resume_stage(unsigned stage)
{
    if(control->resume_stage==stage)return;
    control->resume_stage=stage;
    ra_probe_log_printf(&logger,"RESUME_STAGE epoch=%u stage=%u flags=0x%08X\n",sleep_epoch,stage,sleep_flags);
}
static void resume_service(void)
{
    unsigned flags=__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE);
    uint64_t old_deadline;char hash[33];unsigned char code[64];int rc=0;
    unsigned epoch=sleep_epoch;
    ra_session_stats stats;
    if(!(flags&2u) || control->stop)return;
    if(ra_mail_phase(&mailbox)!=RA_MAIL_IDLE || __atomic_load_n(&report_ready,__ATOMIC_ACQUIRE)) {
        resume_stage(2);return;
    }
    recovery_busy=1;engine_stop();ui_menu=0;
    resume_stage(3);
    if(ui_display.active) {
        ui_present_menu(now_ms(NULL));
        if(ui_display.active){recovery_busy=0;return;}
    }
    ui_network_notice(15,0);
    old_deadline=deadline;deadline=now_ms(NULL)+100000u;
    if(resume_code_ready) {
        resume_stage(4);
        if(!ra_game_map_valid(&game_reader.map) || memcmp(&control->game_map,&game_reader.map,sizeof(game_reader.map)) ||
           !control->probe_installed || read_game_segment(0x08842D90u,code,sizeof(code),NULL)!=sizeof(code) ||
           memcmp(code,resume_code,sizeof(code)))rc=-35433;
        if(!rc)rc=disc_hash(hash);
        ra_session_get_stats(runtime,&stats);
        if(!rc && stats.game_loaded && !stats.load_pending) {
            resume_stage(5);
            if(!ra_session_resume(runtime))rc=-35433;
        }
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
        resume_stage(6);
        ra_probe_log_printf(&logger,"RESUME_OK memory=%s history=RESET_NO_CATCHUP pending=RETAINED\n",resume_code_ready?"VERIFIED":"STARTUP_VALIDATION_PENDING");
    }
    recovery_busy=0;
}
/* Keep every startup/observation caller parked while suspend or recovery is
 * pending. Returning success here used to permit outer logs/UI during sleep. */
static int check(void)
{
    for(;;) {
        if(cancelled(NULL))return RA_HTTPS_CANCEL;
        if(recovery_busy && __atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE)==1u)return RA_HTTPS_CANCEL;
        if(!__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE) || recovery_busy)break;
        sceKernelDelayThread(20000);
    }
    return now_ms(NULL)>=deadline?RA_HTTPS_TIMEOUT:0;
}
#endif
