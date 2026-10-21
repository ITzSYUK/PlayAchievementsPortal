#ifndef RA_LOG_LIFECYCLE_H
#define RA_LOG_LIFECYCLE_H
/* Only the owning worker writes. Reserve the end of the bounded log for
 * shutdown even after normal diagnostics have reached their limit. */
#define RA_LOG_STOP_RESERVE 2048u
static int log_lifecycle;
static uint32_t log_session;
static void log_stage(const char* stage, int result)
{
    int saved = log_lifecycle;
    log_lifecycle = 1;
    ra_probe_log_printf(&logger, "LIFECYCLE session=%u uptime_us32=%u stage=%s result=%d\n",
        (unsigned)log_session, (unsigned)sceKernelGetSystemTimeLow(), stage, result);
    log_lifecycle = saved;
}
#endif
