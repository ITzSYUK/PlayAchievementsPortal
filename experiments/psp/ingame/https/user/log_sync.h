#ifndef RA_LOG_SYNC_H
#define RA_LOG_SYNC_H
/* Worker only, never the game callback or transport. Device sync does not
 * establish why a previous log ended; it only requests periodic writeback. */
static uint64_t log_sync_next;
static unsigned log_sync_calls, log_sync_max_ms;
static int log_sync_last_result;
static int sync_logs(int force)
{
    uint64_t started, elapsed;
    if (log_fd < 0) return 0;
    started = now_ms(NULL);
    if (!force && started < log_sync_next) return 0;
    log_sync_last_result = sceIoSync("ms0:",0);
    elapsed = now_ms(NULL)-started;
    ++log_sync_calls;
    if (elapsed > log_sync_max_ms) log_sync_max_ms = elapsed > UINT32_MAX ? UINT32_MAX : (unsigned)elapsed;
    log_sync_next = now_ms(NULL)+5000u;
    return log_sync_last_result < 0 ? log_sync_last_result : 0;
}
#endif
