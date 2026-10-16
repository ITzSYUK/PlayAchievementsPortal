#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <string.h>
#include <errno.h>
#include "client_shared.h"
#include "ra_client_session.h"
#include "probe_log.h"
PSP_MODULE_INFO("RAClientUser", 0, 0, 10);
PSP_HEAP_SIZE_KB(32);
typedef char psp_format_abi_must_be_32bit[(sizeof(int) == 4 && sizeof(long) == 4 && sizeof(uint32_t) == 4) ? 1 : -1];
extern void __libcglue_init(int, char**);
extern void __libcglue_deinit(void);
/* PSP SDK's weak pthread initializer is unnecessary for this single owner. */
void __libpthreadglue_init(void) {}
/* Runtime uses only explicit PSP I/O. Default newlib read/write/close adapters
 * also import NetInet socket stubs even without network initialization.
 * Reject libc descriptor I/O here; never close the game's stdio descriptors. */
int _read(int fd, void* bytes, size_t size)
{ (void)fd; (void)bytes; (void)size; errno = ENOSYS; return -1; }
int _write(int fd, const void* bytes, size_t size)
{ (void)fd; (void)bytes; (void)size; errno = ENOSYS; return -1; }
int _close(int fd) { (void)fd; errno = ENOSYS; return -1; }
static SceUID thread_id = -1, queue_lock = -1, log_fd = -1;
static ra_client_control* control;
static ra_probe_log logger;
static unsigned log_bytes;
void _exit(int result) __attribute__((noreturn));
void _exit(int result)
{
    static const char message[] = "FATAL runtime exit; resources retained; game exit not requested\n";
    /* A libc fatal exit must neither exit the game nor claim safe cleanup. */
    if (control) { control->result = result ? result : -35001; control->stop = 1; }
    if (log_fd >= 0) sceIoWrite(log_fd, message, sizeof(message) - 1);
    sceKernelExitThread(result);
    for (;;) sceKernelDelayThread(100000);
}
static int log_write(const void* data, unsigned size, void* unused)
{
    int rc;
    (void)unused;
    if (size > 16384u - log_bytes) return -35002;
    rc = sceIoWrite(log_fd, data, size);
    if (rc > 0) log_bytes += (unsigned)rc;
    return rc;
}
static uint64_t now_ms(void* unused) { (void)unused; return sceKernelGetSystemTimeWide() / 1000; }
static void lock_queue(void* unused) { (void)unused; if (sceKernelWaitSema(queue_lock, 1, NULL) < 0) _exit(-35003); }
static void unlock_queue(void* unused) { (void)unused; if (sceKernelSignalSema(queue_lock, 1) < 0) _exit(-35003); }
static int worker(SceSize args, void* argp)
{
    char* argv[] = { RA_CLIENT_DIR "RAClientUser.prx", NULL };
    ra_session_config config = { NULL, now_ms, NULL, lock_queue, unlock_queue };
    ra_client_session* session = NULL;
    ra_session_stats stats;
    int result = 0, cleanup_result = 0;
    unsigned samples = 0, libc_ready = 0;
    (void)args; (void)argp;
    log_fd = sceIoOpen(RA_CLIENT_DIR "client-user.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0) { result = log_fd; goto done; }
    ra_probe_log_init(&logger, log_write, NULL);
    ra_probe_log_printf(&logger, "client_version=0.10 rc_client=12.5.0 heap_bytes=%u stack_bytes=%u\n",
        RA_CLIENT_HEAP_BYTES, RA_CLIENT_STACK_BYTES);
    ra_probe_log_printf(&logger, "begin libc init; fixed heap, no pthread worker\n");
    if (logger.error) { result = logger.error; goto done; }
    __libcglue_init(1, argv); libc_ready = 1;
    ra_probe_log_printf(&logger, "libc init returned\n");
    if (logger.error) { result = logger.error; goto done; }
    queue_lock = sceKernelCreateSema("RAClientQueue", 0, 1, 1, NULL);
    if (queue_lock < 0) { result = queue_lock; goto done; }
    session = ra_session_create(&config);
    if (!session) { result = -35004; goto done; }
    control->ready = 1;
    ra_probe_log_printf(&logger, "RC_CLIENT_READY spectator=1 hardcore=0 frames=DISABLED transport=UNAVAILABLE memory_reader=UNAVAILABLE\n");
    ra_probe_log_printf(&logger, "This build does not log in, load a set or submit awards; rc_client runs idle on its owner thread.\n");
    while (!control->stop && !logger.error) {
        /* No timer/VBlank substitution for game frames. No credentials or
         * network thread are enabled before hardware runtime/TLS validation. */
        ra_session_tick(session, 0, 1);
        ra_session_get_stats(session, &stats);
        control->idle_calls = stats.idle_calls; control->frame_calls = stats.frame_calls;
        if (++samples % 50 == 0)
            ra_probe_log_printf(&logger, "client_idle calls=%u frames=%u pending=%u buffer_bytes=%u\n",
                stats.idle_calls, stats.frame_calls, stats.pending, (unsigned)stats.buffer_bytes);
        sceKernelDelayThread(20000);
    }
    result = logger.error;
done:
    if (session && !ra_session_close(session)) cleanup_result = -35005;
    if (!cleanup_result && queue_lock >= 0) {
        cleanup_result = sceKernelDeleteSema(queue_lock);
        if (cleanup_result >= 0) queue_lock = -1;
    }
    if (log_fd >= 0) {
        ra_probe_log_printf(&logger, "client_finished result=%d cleanup_result=%d log_error=0x%08X\n",
            result, cleanup_result, (unsigned)logger.error);
        int close_result = sceIoClose(log_fd);
        if (close_result < 0) cleanup_result = close_result;
        else log_fd = -1;
    }
    control->result = cleanup_result < 0 ? cleanup_result : result;
    if (cleanup_result < 0) { control->stop = 1; return cleanup_result; }
    if (libc_ready) __libcglue_deinit();
    control->done = 1;
    return result;
}
int module_start(SceSize args, void* argp)
{
    ra_client_args request;
    uintptr_t address;
    int rc;
    if (args != sizeof(request) || !argp) return -35006;
    memcpy(&request, argp, sizeof(request)); address = (uintptr_t)request.control;
    if (address < 0x08800000u || address > 0x0A000000u - sizeof(*control) || address % 4) return -35006;
    control = request.control;
    if (control->magic != RA_CLIENT_MAGIC || control->size != sizeof(*control)) { control = NULL; return -35006; }
    thread_id = sceKernelCreateThread("RAClientOwner", worker, 0x60, RA_CLIENT_STACK_BYTES, PSP_THREAD_ATTR_USER, NULL);
    if (thread_id < 0) return thread_id;
    rc = sceKernelStartThread(thread_id, 0, NULL);
    if (rc < 0 && sceKernelDeleteThread(thread_id) >= 0) thread_id = -1;
    return rc;
}
int module_stop(SceSize args, void* argp)
{
    SceUInt timeout = 3000000;
    int rc;
    (void)args; (void)argp;
    if (control) control->stop = 1;
    if (thread_id < 0) return 0;
    rc = sceKernelWaitThreadEnd(thread_id, &timeout);
    if (rc < 0 || !control || !control->done) return rc < 0 ? rc : -35007;
    rc = sceKernelDeleteThread(thread_id);
    if (rc >= 0) { thread_id = -1; control = NULL; }
    return rc;
}
