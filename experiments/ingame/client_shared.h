#ifndef RA_CLIENT_SHARED_H
#define RA_CLIENT_SHARED_H
#include <stdint.h>
#define RA_CLIENT_DIR "ms0:/seplugins/RAClientLite/"
#define RA_CLIENT_MAGIC UINT32_C(0x52414339)
#ifdef RA_CLIENT_HTTPS
#include "ra_game_memory.h"
#define RA_CLIENT_MINOR 49
#define RA_CLIENT_VERSION "0.49"
#define RA_CLIENT_RUNTIME "rc_client-softcore"
#define RA_CLIENT_STATIC_AUDIT_ONLY 0
#define RA_CLIENT_RUNTIME_PROBE_ONLY 0
#define RA_CLIENT_HOOK_ENABLED 1
#define RA_CLIENT_GATE_TIMEOUT_MS 180000u
#define RA_CLIENT_RUNTIME_PROBE_SECONDS 120u
#define RA_CLIENT_LOAD_SECOND 0u
/* Preflight only, not a reservation. Reject the observed late ~844KiB state. */
#define RA_CLIENT_EARLY_FREE UINT32_C(2097152)
#define RA_CLIENT_REQUEST_SECOND 10u
#define RA_CLIENT_OBSERVE_SECONDS 720u
#define RA_CLIENT_BOOT_LOG_LIMIT 131072u
#define RA_CLIENT_EXIT_LOG_RESERVE 1024u
#define RA_CLIENT_SAMPLE_MS 0u
#define RA_CLIENT_USER_SIZE UINT32_C(0x02800000)
#define RA_CLIENT_HEAP_BYTES 2097152u
#define RA_CLIENT_STACK_BYTES 32768u
#else
#define RA_CLIENT_MINOR 10
#define RA_CLIENT_OBSERVE_SECONDS 120u
#define RA_CLIENT_BOOT_LOG_LIMIT 32768u
#define RA_CLIENT_VERSION "0.10"
#define RA_CLIENT_RUNTIME "rc_client"
#define RA_CLIENT_LOAD_SECOND 10u
#define RA_CLIENT_USER_SIZE UINT32_C(0x01800000)
#define RA_CLIENT_HEAP_BYTES 32768u
#define RA_CLIENT_STACK_BYTES 16384u
#endif
#define RA_CLIENT_USER_BASE UINT32_C(0x08800000)
/* Only the selected layout is accepted. Extra partition 9 is not
 * treated as executable user memory. ARK +16MiB extends partition 2 to 40MiB. */
static inline int ra_client_partition_valid(uint32_t address, uint32_t size, uint32_t attr)
{ return address == RA_CLIENT_USER_BASE && size == RA_CLIENT_USER_SIZE && (attr & 3u); }
static inline int ra_client_user_range(uint32_t address, uint32_t size)
{
    return size && address >= RA_CLIENT_USER_BASE &&
        address - RA_CLIENT_USER_BASE < RA_CLIENT_USER_SIZE &&
        size <= RA_CLIENT_USER_SIZE - (address - RA_CLIENT_USER_BASE);
}
#define RA_CLIENT_ENTRY_STACK 8192u
#ifdef RA_CLIENT_HTTPS
/* 0.49: measured PRX fits 376 KiB PT_LOAD / 436 KiB file reserves.
 * Reserves rounded to 4 KiB; heap and free floor stay unchanged.
 * Pending snapshots/wire are bounded BSS; DNS retains its 8 KiB stack. */
#define RA_CLIENT_MODULE_LIMIT 385024u
#define RA_CLIENT_FILE_LIMIT 446464u
#else
#define RA_CLIENT_MODULE_LIMIT 294912u
#define RA_CLIENT_FILE_LIMIT 327680u
#endif
#define RA_CLIENT_FREE_FLOOR 196608u
#define RA_CLIENT_START_FREE (RA_CLIENT_MODULE_LIMIT + RA_CLIENT_FILE_LIMIT + RA_CLIENT_FREE_FLOOR + 4096u)
#ifdef RA_CLIENT_HTTPS
#define RA_STOP_PENDING 0x7FFFFFFF
#define RA_EXIT_USER_REBOOT 1u
#define RA_EXIT_USER_STOP 2u
enum { RA_STOP_NONE, RA_STOP_ENTER, RA_STOP_WAIT, RA_STOP_WAIT_ERROR,
       RA_STOP_NOT_DONE, RA_STOP_DELETE, RA_STOP_DELETE_ERROR, RA_STOP_FINISHED,
       RA_STOP_NO_THREAD, RA_STOP_REFER_ERROR, RA_STOP_THREAD_RUNNING };
#endif
typedef struct {
    uint32_t magic, size;
    volatile uint32_t stop, ready, done, idle_calls, frame_calls;
    volatile int result;
#ifdef RA_CLIENT_HTTPS
    uint32_t log_session;
    volatile uint32_t exit_user_flags;
    volatile uint32_t request_allowed;
    /* Immutable game map published before request_allowed. No callback or
     * kernel function pointer is passed across the privilege boundary. */
    ra_game_map game_map;
    unsigned char entropy[80];
    int entropy_status, foreign_network;
    /* Module-stop diagnostics use shared storage; the user log is closed. */
    volatile int owner_thread_id, stop_caller_id, stop_stage;
    volatile int stop_wait_result, stop_delete_result;
    volatile int stop_refer_result, stop_thread_status, stop_exit_status;
    volatile uint32_t stop_timeout_us, free_before_deinit, free_after_deinit;
    /* One temporary data-slot hook. The kernel validates the wrapper and
     * restores the original slot before the user PRX can be unloaded. */
    volatile uint32_t probe_wrapper, probe_installed, probe_active;
    volatile uint32_t probe_calls, probe_object;
    volatile int32_t probe_last_result;
    volatile uint32_t notice_wrapper,notice_installed,notice_active;
#endif
} ra_client_control;
typedef struct { ra_client_control* control; } ra_client_args;
#endif
