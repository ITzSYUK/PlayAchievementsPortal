#ifndef RA_RESUME_LOG_H
#define RA_RESUME_LOG_H
/* Owner only, after RESUMING + settling interval, before other storage I/O.
 * Reopen the existing session append log; never rotate/create/truncate it. */
static unsigned resume_log_epoch=UINT32_MAX,resume_log_ready;
static uint64_t resume_log_until,resume_log_next;
static int resume_log_prior,resume_log_close;
static int resume_log_prepare(void)
{
    int fd;
    uint64_t now=now_ms(NULL);
    if(resume_log_epoch!=sleep_epoch) {
        resume_log_epoch=sleep_epoch;resume_log_ready=0;
        resume_log_until=now+5000u;resume_log_next=0;
        resume_log_prior=logger.error;
        resume_log_close=log_fd>=0?sceIoClose(log_fd):0;log_fd=-1;
        control->resume_stage=1;
    }
    if(resume_log_ready)return 1;
    if(now<resume_log_next)return 0;
    fd=sceIoOpen(RA_CLIENT_DIR "client-user.log",PSP_O_WRONLY|PSP_O_APPEND,0);
    if(fd>=0) {
        log_fd=fd;logger.error=0;log_sync_next=now+5000u;
        ra_probe_log_printf(&logger,"POWER_WAKE epoch=%u flags=0x%08X prior_log_error=%d old_close=%d\n",
            sleep_epoch,sleep_flags,resume_log_prior,resume_log_close);
        if(!logger.error){resume_log_ready=1;return 1;}
        fd=logger.error;sceIoClose(log_fd);log_fd=-1;
    }
    if(now>=resume_log_until){control->result=fd;control->stop=1;}
    resume_log_next=now+100u;return 0;
}
#endif
