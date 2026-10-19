#ifndef RA_EXIT_NOTICE_H
#define RA_EXIT_NOTICE_H
/* Separate append descriptor: the observer may be asleep or its logger failed.
 * No logger/heap/module-manager calls, no join, no game callback replacement.
 * The reserved budget includes failed writes, so retries cannot grow the log.
 * Source ABI/order: uOFW 28306bb5, modulemgr/rebootModule.c + loadexec.c. */
static unsigned exit_notice_bytes;
static volatile int exit_log_ready;
static int exit_notice_busy, exit_notice_last_result;
static int exit_notice(const char* source, int result)
{
    char line[384];
    uint32_t flags = 0, done = 0;
    uint32_t user_stop=0,power_flags=0,resume_stage=0;
    int user_result=0;
    unsigned session;
    int stage = 0, intr, length, fd, rc = 0, close_rc;
    unsigned offset = 0;
    intr = sceKernelCpuSuspendIntr();
    if (exit_notice_busy || !exit_log_ready || !log_session) {
        sceKernelCpuResumeIntr(intr); return -1;
    }
    exit_notice_busy = 1;
    session = log_session;
    /* cleanup clears control with interrupts masked before freeing it. */
    if (control && control->magic == RA_CLIENT_MAGIC && control->size == sizeof(*control)) {
        flags = control->exit_user_flags;
        done = control->done;
        stage = control->stop_stage;
        user_stop=control->stop;user_result=control->result;
        power_flags=control->power_flags;resume_stage=control->resume_stage;
    }
    sceKernelCpuResumeIntr(intr);
    length = ra_kernel_snprintf(line, sizeof(line),
        "EXIT_NOTICE session=%u uptime_us32=%u source=%s result=%d user_flags=%u worker_done=%u stop_stage=%d prior_io=%d user_stop=%u user_result=%d power_flags=0x%08X resume_stage=%u\n",
        session, (unsigned)sceKernelGetSystemTimeLow(), source, result,
        (unsigned)flags, (unsigned)done, stage, exit_notice_last_result,
        (unsigned)user_stop,user_result,(unsigned)power_flags,(unsigned)resume_stage);
    if (length <= 0 || (unsigned)length >= sizeof(line) ||
        (unsigned)length > RA_CLIENT_EXIT_LOG_RESERVE - exit_notice_bytes) { rc = -1; goto finish; }
    exit_notice_bytes += (unsigned)length;
    /* Never create/truncate a log from teardown; no stale-session fallback. */
    fd = sceIoOpen(RA_CLIENT_DIR "client-bootstrap.log", PSP_O_WRONLY | PSP_O_APPEND, 0);
    if (fd < 0) { rc = fd; goto finish; }
    while (offset < (unsigned)length) {
        rc = sceIoWrite(fd, line + offset, (unsigned)length - offset);
        if (rc <= 0 || (unsigned)rc > (unsigned)length - offset) {
            if (rc >= 0) rc = -1;
            break;
        }
        offset += (unsigned)rc;
    }
    if (offset == (unsigned)length) rc = 0;
    close_rc = sceIoClose(fd);
    if (!rc && close_rc < 0) rc = close_rc;
    /* No new kernel import for diagnostics. Close is best effort; the user
     * worker retains its existing periodic sync. Never veto reboot. */
finish:
    exit_notice_last_result = rc;
    intr = sceKernelCpuSuspendIntr();
    exit_notice_busy = 0;
    sceKernelCpuResumeIntr(intr);
    return rc;
}
#endif
