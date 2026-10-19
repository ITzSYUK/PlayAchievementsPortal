#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <psputility.h>
#include <psputility_netparam.h>
#include <pspwlan.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psprtc.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "client_shared.h"
#include "client_mode.h"
#include "reconnect_policy.h"
#include "ra_https.h"
#include "ra_http_body.h"
#include "ra_net_resources.h"
#include "ra_client_session.h"
#include "ra_auth_file.h"
#include "ra_game_memory.h"
#include "probe_log.h"
#include "probe_power.h"
#include "rtc_epoch.h"
#include "rc_api_runtime.h"
#include "rc_client.h"
#include "rc_error.h"
#include "md5.h"
#include "socket_io.h"
#include "../../../../tls/compact/gts_root.h"
PSP_MODULE_INFO("RAClientUser", 0, 0, RA_CLIENT_MINOR);
PSP_HEAP_SIZE_KB(2048);
typedef char format_abi[(sizeof(int) == 4 && sizeof(long) == 4 && sizeof(uint32_t) == 4) ? 1 : -1];
extern void __libcglue_init(int, char**);
extern void __libcglue_deinit(void);
void __libpthreadglue_init(void) {}
/* This worker uses only explicit PSP file/socket I/O and UTC from RTC.
 * Keep newlib heap/locks, omit descriptor tables, CWD and timezone parsing. */
void __timezone_update(void) {}
void __init_cwd(char* path) { (void)path; }
void __fdman_init(void) {}
int _read(int fd, void* bytes, size_t size) { (void)fd; (void)bytes; (void)size; errno = ENOSYS; return -1; }
int _write(int fd, const void* bytes, size_t size) { (void)fd; (void)bytes; (void)size; errno = ENOSYS; return -1; }
int _close(int fd) { (void)fd; errno = ENOSYS; return -1; }
int _fstat(int fd, struct stat* out) { (void)fd; (void)out; errno = ENOSYS; return -1; }
off_t _lseek(int fd, off_t offset, int origin) { (void)fd; (void)offset; (void)origin; errno = ENOSYS; return -1; }
int _isatty(int fd) { (void)fd; errno = ENOSYS; return 0; }
static ra_client_control* control;
static SceUID thread_id = -1, log_fd = -1, auth_fd = -1;
static int socket_fd = -1;
static ra_probe_log logger;
#include "log_lifecycle.h"
static ra_probe_power power_owner;
static unsigned written, entropy_offset;
static u64 epoch_tick;
static u32 resolution;
static uint64_t deadline;
static unsigned char* response_body;
static ra_net_resources network;
static ra_client_session* runtime;
static ra_game_reader game_reader;
static SceUID queue_guard = -1, transport_thread = -1;
static volatile int transport_stopping, transport_done;
static int transport_started;
static uint64_t transport_deadline;
static unsigned transport_ticket;
/* Sampled numeric transport diagnostics; no pointers or borrowed buffers. */
enum { IO_DNS_CREATE=1, IO_DNS_LOOKUP, IO_SOCKET, IO_SOCKET_SETUP, IO_CONNECT,
       IO_CONNECT_ERROR, IO_TLS, IO_SEND, IO_RECV, IO_POLL, IO_CLOSE, IO_DNS_DELETE, IO_DNS_STOP };
static uint32_t transport_location,transport_actions;
static void io_enter(unsigned call)
{
    __atomic_fetch_add(&transport_actions,1,__ATOMIC_RELAXED);
    __atomic_store_n(&transport_location,call*2+1,__ATOMIC_RELEASE);
}
static void io_leave(unsigned call)
{ __atomic_store_n(&transport_location,call*2,__ATOMIC_RELEASE); }
static uint64_t now_ms(void* unused);
static int transport_check(void);
#include "dns_job.h"
static mbedtls_ctr_drbg_context entropy_rng;
typedef struct {
    int code, io_cleanup,retryable;
    unsigned ticket, kind, elapsed_ms;
    ra_socket_mode socket_mode;
    ra_dns_report dns;
    ra_https_result https;
} transport_report;
static transport_report report;
static uint32_t report_ready;
#include "session_mailbox.h"
static ra_session_mailbox mailbox;
static transport_report mailbox_finished;
static void session_owner_tick(void);
static void pending_capture(void);
static void pending_service(void);
#include "frame_timeline.h"
static ra_frame_timeline timeline;
static volatile uint32_t timeline_lock, timeline_missed;
#include "frame_engine.h"
static unsigned online_mode;
static unsigned encore_mode;
static unsigned sample_ms = RA_CLIENT_SAMPLE_MS;
static unsigned wifi_online;
static unsigned stop_from_module,power_stop_flags;
static uint32_t sleep_events;
static unsigned sleep_epoch;
static unsigned sleep_was_active,recovery_busy;
static uint64_t sleep_started;
static unsigned char resume_code[64];
static int resume_code_ready;
static void resume_service(void);
static unsigned http_outage;
static uint64_t now_ms(void* unused);
#include "online_ui.h"
static ra_timeline_point display_point(void)
{
    ra_timeline_point p;
    void* framebuffer=NULL;int stride=0,format=0;
    p.us=sceKernelGetSystemTimeLow();
    p.vcount=(uint32_t)sceDisplayGetVcount();
    p.valid=sceDisplayGetFrameBuf(&framebuffer,&stride,&format,PSP_DISPLAY_SETBUF_IMMEDIATE)>=0;
    p.framebuffer=(uint32_t)(uintptr_t)framebuffer;p.stride=(uint32_t)stride;p.format=(uint32_t)format;
    return p;
}
typedef int (*ra_game_input_method)(void* object);
int ra_runtime_input_probe(void* object) __attribute__((noinline));
int ra_runtime_input_probe(void* object)
{
    int result;
    ra_client_control* shared = control;
    ra_timeline_point before,after;
    if(!shared)return ((ra_game_input_method)(uintptr_t)UINT32_C(0x08842D90))(object);
    __sync_add_and_fetch(&shared->probe_active,1);
    __sync_add_and_fetch(&shared->probe_calls,1);
    shared->probe_object=(uint32_t)(uintptr_t)object;
    __sync_synchronize();
    before=display_point();
    result=((ra_game_input_method)(uintptr_t)UINT32_C(0x08842D90))(object);
    after=display_point();
    engine_frame();
    /* No logger, allocation, waits or locks held across the original call. */
    if(!__sync_lock_test_and_set(&timeline_lock,1)){
        ra_timeline_add(&timeline,&before,&after);
        __sync_lock_release(&timeline_lock);
    }else __sync_add_and_fetch(&timeline_missed,1);
    /* Explicit menu pause, outside rc_client/queue/timeline ownership. */
    ui_pause_point();
    shared->probe_last_result=result;
    __sync_sub_and_fetch(&shared->probe_active,1);
    return result;
}
void _exit(int result) __attribute__((noreturn));
static void queue_lock(void* unused)
{
    int rc; (void)unused;
    if (frame_queue_owner == sceKernelGetThreadId()) return;
    rc = sceKernelWaitSema(queue_guard, 1, NULL);
    if (rc < 0) _exit(-35401); /* Do not continue without the lock or free shared storage. */
}
static void queue_unlock(void* unused)
{
    int rc; (void)unused;
    if (frame_queue_owner == sceKernelGetThreadId()) return;
    rc = sceKernelSignalSema(queue_guard, 1);
    if (rc < 0) _exit(-35401);
}
static void session_owner_tick(void)
{
    /* Only the serialized owner touches adapter queues. Network never holds
     * queue_guard, so the game cannot collide with transport polling/logging. */
    if (ra_mail_phase(&mailbox) == RA_MAIL_RESPONSE &&
        !__atomic_load_n(&report_ready, __ATOMIC_ACQUIRE)) {
        size_t received = mailbox.size;
        report = mailbox_finished;
        if (ra_mail_receive(&mailbox, runtime, response_body) < 0) {
            control->result = -35403; control->stop = 1;
        }
        if (received <= RA_SESSION_RESPONSE_LIMIT) mbedtls_platform_zeroize(response_body, received);
        __atomic_store_n(&report_ready, 1, __ATOMIC_RELEASE);
    }
    ra_session_tick(runtime, engine_enabled && frame_queue_owner == sceKernelGetThreadId(),
                    !(engine_enabled && frame_queue_owner == sceKernelGetThreadId()));
    ui_owner_events();
    pending_capture();
    if (!transport_stopping && !control->stop && !__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE) && __atomic_load_n(&wifi_online,__ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&report_ready, __ATOMIC_ACQUIRE)) ra_mail_offer(&mailbox, runtime);
}
static int log_write(const void* data, unsigned size, void* unused)
{
    int rc; (void)unused;
    /* Full diagnostic log does not stop a long session. Real I/O errors do. */
    unsigned limit = 131072u - (log_lifecycle ? 0 : RA_LOG_STOP_RESERVE);
    if (written >= limit || size > limit - written) return (int)size;
    rc = sceIoWrite(log_fd, data, size); if (rc > 0) written += (unsigned)rc; return rc;
}
void _exit(int result) __attribute__((noreturn));
void _exit(int result)
{
    if (control) { control->result = result ? result : -35304; control->stop = 1; }
    sceKernelExitThread(0); for (;;) sceKernelDelayThread(100000);
}
static int record(const char* label, int rc)
{ ra_probe_log_printf(&logger, "%s: 0x%08X\n", label, (unsigned)rc); return rc; }
static uint64_t now_ms(void* unused) { (void)unused; return sceKernelGetSystemTimeWide() / 1000; }
#include "log_sync.h"
static int run_runtime_probe(void)
{
    uint32_t previous=0;unsigned second;
    control->probe_wrapper=(uint32_t)(uintptr_t)&ra_runtime_input_probe;
    __sync_synchronize();control->ready=1;
    ra_probe_log_printf(&logger,"RUNTIME_PROBE_READY wrapper=0x%08X auth_read=0 requests=0 frames=0 duration_s=%u\n",
        (unsigned)control->probe_wrapper,RA_CLIENT_RUNTIME_PROBE_SECONDS);
    deadline=now_ms(NULL)+RA_CLIENT_GATE_TIMEOUT_MS;
    while(!control->request_allowed){
        if(control->stop)return -35204;
        if(now_ms(NULL)>=deadline)return -35203;
        sceKernelDelayThread(10000);
    }
    for(second=0;second<=RA_CLIENT_RUNTIME_PROBE_SECONDS;++second){
        uint32_t calls=control->probe_calls;unsigned i;
        ra_frame_timeline copy;
        while(__sync_lock_test_and_set(&timeline_lock,1)){
            if(control->stop)return -35204;
            sceKernelDelayThread(1000);
        }
        copy=timeline;timeline.traces=0;
        __sync_lock_release(&timeline_lock);
        ra_probe_log_printf(&logger,"TIMELINE second=%u us=%u calls=%u delta=%u captured=%u missed=%u duplicate=%u skipped=%u resets=%u fb_changes=%u errors=%u crossed=%u duration_max_us=%u interval_max_us=%u\n",
            second,sceKernelGetSystemTimeLow(),calls,calls-previous,copy.calls,(unsigned)timeline_missed,
            copy.duplicate_vcount,copy.skipped_vcount,copy.reset_vcount,copy.framebuffer_changes,
            copy.read_errors,copy.crossed_vblank,copy.duration_max_us,copy.interval_max_us);
        for(i=0;i<copy.traces;++i){
            const ra_timeline_trace* t=&copy.trace[i];
            ra_probe_log_printf(&logger,"TRACE second=%u before_us=%u after_us=%u before_v=%u after_v=%u before_fb=0x%08X after_fb=0x%08X valid=%u\n",
                second,t->before.us,t->after.us,t->before.vcount,t->after.vcount,
                t->before.framebuffer,t->after.framebuffer,t->before.valid && t->after.valid);
        }
        previous=calls;
        if(second==RA_CLIENT_RUNTIME_PROBE_SECONDS)break;
        if(control->stop)return -35204;
        sceKernelDelayThread(1000000);
    }
    ra_probe_log_printf(&logger,"RUNTIME_PROBE_DONE calls=%u auth_read=0 requests=0 frames=0 binding=UNPROVEN\n",
        (unsigned)control->probe_calls);
    return logger.error;
}
static void memory_mark(const char* operation, const char* phase)
{
    ra_probe_log_printf(&logger, "net_memory operation=%s phase=%s user_free=%u user_largest=%u\n",
        operation, phase, (unsigned)sceKernelTotalFreeMemSize(), (unsigned)sceKernelMaxFreeMemSize());
}
static void heap_mark(const char* phase)
{
    struct mallinfo usage = mallinfo();
    ra_probe_log_printf(&logger, "client_heap phase=%s arena=%u allocated=%u free_in_arena=%u configured_limit=%u\n",
                        phase, (unsigned)usage.arena, (unsigned)usage.uordblks,
                        (unsigned)usage.fordblks, RA_CLIENT_HEAP_BYTES);
}
static void drain_transport_report(void)
{
    transport_report copy;
    if (queue_guard < 0) return;
    if (!__atomic_load_n(&report_ready, __ATOMIC_ACQUIRE)) return;
    copy = report;
    __atomic_store_n(&report_ready, 0, __ATOMIC_RELEASE);
    ra_probe_log_printf(&logger,"HTTP_TRANSFER ticket=%u kind=%u elapsed_ms=%u\n",copy.ticket,copy.kind,copy.elapsed_ms);
    ra_probe_log_printf(&logger,"DNS_RESULT ticket=%u elapsed_ms=%u result=%d stop_calls=%u stop_result=%d\n",
        copy.ticket,copy.dns.elapsed_ms,copy.dns.result,copy.dns.stop_calls,copy.dns.stop_result);
    ra_probe_log_printf(&logger,"SOCKET_MODE ticket=%u set_result=%d read_result=%d value=%d size=%u\n",
        copy.ticket,copy.socket_mode.set_result,copy.socket_mode.read_result,copy.socket_mode.value,copy.socket_mode.size);
    if(copy.code)ra_probe_log_printf(&logger,"HTTP_FAILURE ticket=%u code=%d retry_allowed=%u\n",copy.ticket,copy.code,(unsigned)copy.retryable);
    if((copy.code && copy.retryable) || (!copy.code && ra_startup_retry(RC_API_FAILURE,copy.https.status))) {
        unsigned reason=ra_ui_transport_reason(copy.code,copy.https.status,copy.dns.result,
            copy.https.stage,copy.https.verify_flags,copy.kind);
        ui_network_error(8,0,reason,copy.code?copy.code:copy.https.status);
        http_outage=1;
    } else if(copy.code || copy.https.status>=400) {
        ui_network_error(12,0,ra_ui_transport_reason(copy.code,copy.https.status,copy.dns.result,
            copy.https.stage,copy.https.verify_flags,copy.kind),
            copy.code?copy.code:copy.https.status);
    } else if(http_outage && !copy.code && copy.https.status==200) {
        http_outage=0;ui_network_notice(13,0);record("RA_SERVER_REACHABLE",0);
    }
    ra_probe_log_printf(&logger, "SESSION_HTTP stage=%s code=%d verify_flags=0x%08X http=%d body_bytes=%u tls_heap_peak=%u tls_heap_remaining=%u io_cleanup=%d\n",
        copy.https.stage ? copy.https.stage : "transport", copy.code, (unsigned)copy.https.verify_flags,
        copy.https.status, (unsigned)copy.https.body_size, (unsigned)copy.https.heap_peak,
        (unsigned)copy.https.heap_remaining, copy.io_cleanup);
    ra_probe_log_printf(&logger, "HTTP_DIAG ticket=%u error=%s phase=%s received=%u declared=%u has_length=%d number_overflow=%d chunked=%d encoding=%s wire=%u headers=%u trailers=%u chunk_meta=%u chunks=%u last_chunk=%u line_bytes=%u limit=%u\n",
        copy.ticket, ra_http_error_name(copy.https.http_error), ra_http_phase_name(copy.https.http_phase),
        (unsigned)copy.https.body_received, (unsigned)copy.https.declared_length, copy.https.has_length,
        copy.https.length_overflow, copy.https.chunked, ra_http_encoding_name(copy.https.content_encoding), (unsigned)copy.https.wire_bytes,
        (unsigned)copy.https.header_bytes, (unsigned)copy.https.trailer_bytes, (unsigned)copy.https.chunk_meta_bytes,
        (unsigned)copy.https.chunk_count, (unsigned)copy.https.last_chunk, (unsigned)copy.https.line_bytes, RA_RESPONSE_BODY_LIMIT);
}
#include "apctl_observer.h"
#include "reconnect.h"
static void drain_request_trace(void)
{
    ra_request_trace event;
    unsigned count=0;
    /* Startup diagnostics only. Once frames own queue_guard, a worker-side
     * lock here would make engine_frame's nonblocking PollSema fail. Only
     * this worker enables frames; engine_stop drains them before disabling. */
    if(engine_enabled)return;
    /* Bounded drain even if a frame owner keeps producing diagnostics. */
    while(runtime && count++<RA_REQUEST_TRACE_CAPACITY && ra_session_take_request_trace(runtime,&event))
        ra_probe_log_printf(&logger,"REQUEST_TRACE ticket=%u kind=%u stage=%u time_ms32=%u age_ms=%u status=%d\n",
            event.ticket,event.kind,event.stage,event.time_ms,event.age_ms,event.status);
}
static void startup_progress(const char* phase, const ra_session_stats* stats)
{
    static uint64_t next;
    SceKernelThreadInfo info;
    int thread_rc;
    uint64_t now=now_ms(NULL);
    if(now<next)return;
    next=now+5000;
    ra_probe_log_printf(&logger,"STARTUP_WAIT phase=%s time_ms32=%u auth_pending=%d load_pending=%d pending=%u submitted=%u completed=%u result=%d http=%d copy_error=%d mailbox=%u report=%u trace_dropped=%u\n",
        phase,(unsigned)now,stats->auth_pending,stats->load_pending,stats->pending,
        stats->submitted,stats->completed,stats->last_result,stats->last_http_status,
        stats->last_copy_error,ra_mail_phase(&mailbox),
        (unsigned)__atomic_load_n(&report_ready,__ATOMIC_ACQUIRE),stats->request_trace_dropped);
    memset(&info,0,sizeof(info));info.size=sizeof(info);
    thread_rc=sceKernelReferThreadStatus(transport_thread,&info);
    ra_probe_log_printf(&logger,"TRANSPORT_STATE location=%u actions=%u thread_query=%d status=%u wait_type=%d exit_status=%d\n",
        (unsigned)__atomic_load_n(&transport_location,__ATOMIC_ACQUIRE),
        (unsigned)__atomic_load_n(&transport_actions,__ATOMIC_RELAXED),thread_rc,
        thread_rc<0?0:(unsigned)info.status,thread_rc<0?0:info.waitType,thread_rc<0?0:info.exitStatus);
    ra_probe_log_printf(&logger,"DNS_STATE thread=%d done=%u cancel=%u\n",
        __atomic_load_n(&dns_job.thread,__ATOMIC_ACQUIRE),
        (unsigned)__atomic_load_n(&dns_job.done,__ATOMIC_ACQUIRE),
        (unsigned)__atomic_load_n(&dns_job.cancel,__ATOMIC_ACQUIRE));
}
static int cancelled(void* unused)
{
    int event_error, sync_rc;
    (void)unused; sceKernelCheckCallback();
    if(__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE)==1u)return control->stop!=0;
    drain_transport_report();
    drain_request_trace();
    if (runtime && (!engine_enabled || ra_ui_pause_phase(&ui_pause)==RA_UI_PAUSE_ACK || (uint32_t)(sceKernelGetSystemTimeLow()-engine_last_us)>250000u) &&
        !__sync_lock_test_and_set(&engine_gate, 1)) {
        /* While callbacks arrive, they own frame processing and completion
         * delivery. If callbacks stop, keep transport/retries alive via idle.
         * Resumption during this short idle stops the probe on contention. */
        if (!engine_enabled || ra_ui_pause_phase(&ui_pause)==RA_UI_PAUSE_ACK || (uint32_t)(sceKernelGetSystemTimeLow()-engine_last_us) > 250000u) {
            session_owner_tick(); ++control->idle_calls;
        }
        __sync_lock_release(&engine_gate);
    }
    event_error = drain_apctl_events();
    if(event_error && wifi_enabled && !apctl_guard_error) {
        SceUInt timeout=100000;
        int rc=sceKernelWaitSema(apctl_event_lock,1,&timeout);
        if(rc>=0){apctl_history.first_error=0;rc=sceKernelSignalSema(apctl_event_lock,1);}
        if(rc<0){control->result=rc;control->stop=1;}
        else {wifi_policy.reset=1;ui_network_error(8,0,RA_REASON_APCTL,event_error);ra_probe_log_printf(&logger,"WIFI_FAILURE reason=APCTL code=0x%08X retry=scheduled\n",(unsigned)event_error);}
    } else if (event_error) { if (!control->result) control->result = event_error; control->stop = 1; }
    if(!recovery_busy)resume_service();
    if(!__atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE)) {
        wifi_tick();pending_service();
    }
    if(!engine_enabled && ui_ready)ui_poll();
    sync_rc = sync_logs(0);
    if (sync_rc < 0) { record("diagnostic sync failed",sync_rc); control->result=sync_rc; control->stop=1; }
    if (control->stop || logger.error) return 1;
    if ((unsigned)sceKernelTotalFreeMemSize() < RA_CLIENT_FREE_FLOOR ||
        (unsigned)sceKernelMaxFreeMemSize() < RA_CLIENT_ENTRY_STACK) {
        control->stop = 1; control->result = -35305;
        ra_probe_log_printf(&logger, "USER_MEMORY_GUARD free=%u largest=%u\n",
            (unsigned)sceKernelTotalFreeMemSize(), (unsigned)sceKernelMaxFreeMemSize());
        return 1;
    }
    return 0;
}
static int check(void)
{ return cancelled(NULL) ? RA_HTTPS_CANCEL : now_ms(NULL) >= deadline ? RA_HTTPS_TIMEOUT : 0; }
static int power_event(int unused, int flags, void* context);
static int power_create(void* unused) { (void)unused; return sceKernelCreateCallback("RATlsPower", power_event, NULL); }
static int power_register(int slot, int cb, void* unused) { (void)unused; return scePowerRegisterCallback(slot, cb); }
static int power_unregister(int slot, void* unused) { (void)unused; return scePowerUnregisterCallback(slot); }
static int power_delete(int cb, void* unused) { (void)unused; return sceKernelDeleteCallback(cb); }
static const ra_probe_power_api power_api = { power_create, power_register, power_unregister, power_delete, NULL };
static time_t utc(void* unused)
{
    u64 tick; int64_t seconds; time_t out; (void)unused;
    if (!resolution || sceRtcGetCurrentTick(&tick) < 0 || ra_rtc_epoch_seconds(tick, epoch_tick, resolution, &seconds)) return (time_t)-1;
    out = (time_t)seconds; return (int64_t)out == seconds ? out : (time_t)-1;
}
#include "pending_store.h"
static int entropy(void* unused, unsigned char* bytes, size_t size)
{
    (void)unused;
    if (control->entropy_status || size > sizeof(control->entropy) - entropy_offset) return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    memcpy(bytes, control->entropy + entropy_offset, size);
    mbedtls_platform_zeroize(control->entropy + entropy_offset, size); entropy_offset += size; return 0;
}
static int transport_cancelled(void* unused)
{
    (void)unused;
    if (transport_stopping || control->stop || __atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE)) return 1;
    if ((unsigned)sceKernelTotalFreeMemSize() < RA_CLIENT_FREE_FLOOR ||
        (unsigned)sceKernelMaxFreeMemSize() < RA_CLIENT_ENTRY_STACK) {
        control->result = -35305; control->stop = 1; return 1;
    }
    return now_ms(NULL) >= transport_deadline;
}
static int transport_check(void)
{ return now_ms(NULL) >= transport_deadline ? RA_HTTPS_TIMEOUT : transport_cancelled(NULL) ? RA_HTTPS_CANCEL : 0; }
static int tls_entropy(void* unused, unsigned char* bytes, size_t size)
{ (void)unused; return mbedtls_ctr_drbg_random(&entropy_rng, bytes, size); }
static int retryable(int e) { return e == EINTR || e == EAGAIN || e == EWOULDBLOCK; }
static int send_bio(void* unused, const unsigned char* bytes, size_t size)
{
    int rc; (void)unused; if (transport_cancelled(NULL)) return RA_HTTPS_CANCEL;
    io_enter(IO_SEND);rc = ra_socket_send(socket_fd, bytes, size);io_leave(IO_SEND);
    return rc >= 0 ? rc : retryable(sceNetInetGetErrno()) ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
}
static int recv_bio(void* unused, unsigned char* bytes, size_t size)
{
    int rc; (void)unused; if (transport_cancelled(NULL)) return RA_HTTPS_CANCEL;
    io_enter(IO_RECV);rc = ra_socket_recv(socket_fd, bytes, size);io_leave(IO_RECV);
    return rc >= 0 ? rc : retryable(sceNetInetGetErrno()) ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_RECV_FAILED;
}
static int wait_io(void* unused, int writing, unsigned ms)
{
    int rc; (void)unused; if (transport_cancelled(NULL)) return RA_HTTPS_CANCEL;
    io_enter(IO_POLL);rc = ra_socket_poll(socket_fd,writing,ms);io_leave(IO_POLL);
    return rc < 0 && sceNetInetGetErrno() != EINTR ? MBEDTLS_ERR_NET_RECV_FAILED : 0;
}
static int profile(void)
{
    char bytes[8]; unsigned value = 0; int size, i;
    SceUID fd = sceIoOpen(RA_CLIENT_DIR "profile.txt", PSP_O_RDONLY, 0);
    if (fd < 0) return -35306; /* explicit profile required, no silent fallback */
    size = sceIoRead(fd, bytes, sizeof(bytes));
    if (sceIoClose(fd) < 0 || size <= 0 || size == sizeof(bytes)) return -35306;
    for (i = 0; i < size && bytes[i] >= '0' && bytes[i] <= '9'; ++i) value = value * 10 + bytes[i] - '0';
    if (!i || value < 1 || value > 32) return -35306;
    for (; i < size; ++i) if (bytes[i] != '\r' && bytes[i] != '\n') return -35306;
    return (int)value;
}
static int hash_file(md5_state_t* hash, const char* path)
{
    unsigned char bytes[2048];
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    SceOff length;
    int rc = -35307;
    if (fd < 0) return fd;
    length = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (length < 0 || sceIoLseek(fd, 0, PSP_SEEK_SET) < 0) goto end;
    if (length > 64 * 1024 * 1024) length = 64 * 1024 * 1024;
    while (length) {
        unsigned size = length > sizeof(bytes) ? sizeof(bytes) : (unsigned)length;
        rc = check(); if (rc) goto end;
        if (sceIoRead(fd, bytes, size) != (int)size) { rc = -35307; goto end; }
        md5_append(hash, bytes, size); length -= size; sceKernelDelayThread(1000);
    }
    rc = 0;
end:
    if (sceIoClose(fd) < 0 && !rc) rc = -35307;
    return rc;
}
static int disc_hash(char hash[33])
{
    static const char hex[] = "0123456789abcdef";
    unsigned char digest[16]; md5_state_t state; unsigned i; int rc;
    md5_init(&state);
    rc = hash_file(&state, "disc0:/PSP_GAME/PARAM.SFO"); if (rc) return rc;
    rc = hash_file(&state, "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN"); if (rc) return rc;
    md5_finish(&state, digest);
    for (i = 0; i < 16; ++i) { hash[i*2] = hex[digest[i] >> 4]; hash[i*2+1] = hex[digest[i] & 15]; }
    hash[32] = 0;
    /* Previously verified against official rcheevos on the owner's ISO.
     * This is a comparison gate, never a substitute for the bytes read above. */
    return strcmp(hash, "199515744d49ef3620a5f9297c6116f9") ? -35307 : 0;
}
static const int modules[] = { PSP_NET_MODULE_COMMON, PSP_NET_MODULE_INET };
#include "early_network.h"
#include "connect_profile.h"
static int network_release(void* unused, int operation, int arg)
{
    static const char* const labels[] = { "socket_close", "resolver_delete", "apctl_disconnect",
        "apctl_term", "resolver_term", "inet_term", "net_term", "net_module_unload", "apctl_handler_delete" };
    const char* label;
    int rc; (void)unused;
    if (operation < RA_NET_CLOSE || operation > RA_NET_DELETE_APCTL_HANDLER) return -35313;
    if (operation == RA_NET_UNLOAD && (arg < 0 || arg > 1)) return -35313;
    label = operation == RA_NET_UNLOAD ? (arg ? "unload_net_INET" : "unload_net_COMMON") : labels[operation];
    memory_mark(label, "before");
    switch (operation) {
        case RA_NET_CLOSE: rc = sceNetInetClose(arg); if (rc >= 0) socket_fd = -1; break;
        case RA_NET_DELETE_RESOLVER: rc = sceNetResolverDelete(arg); break;
        case RA_NET_DISCONNECT: rc = sceNetApctlDisconnect(); break;
        case RA_NET_DELETE_APCTL_HANDLER: rc = sceNetApctlDelHandler(arg); break;
        case RA_NET_APCTL_TERM: rc = sceNetApctlTerm(); break;
        case RA_NET_RESOLVER_TERM: rc = sceNetResolverTerm(); break;
        case RA_NET_INET_TERM: rc = sceNetInetTerm(); break;
        case RA_NET_TERM: rc = sceNetTerm(); break;
        case RA_NET_UNLOAD: rc = sceUtilityUnloadNetModule(modules[arg]); break;
        default: return -35313;
    }
    ra_probe_log_printf(&logger, "network_cleanup operation=%d argument=%d result=0x%08X\n", operation, arg, (unsigned)rc);
    memory_mark(label, "after");
    return rc;
}
/* Network thread only. It never calls rc_client or writes the owner log. */
static int transport_http(const ra_session_request* request, ra_https_result* result,
    ra_socket_mode* socket_mode,ra_dns_report* dns)
{
    int rc = -1;
    int error = 0;
    socklen_t error_size = sizeof(error);
    struct sockaddr_in address;
    ra_https_options options;
#define STEP(tag,call) do { rc = transport_check(); if (rc) goto end; \
    io_enter(tag);rc = (call);io_leave(tag); \
    if (rc < 0) goto end; } while (0)
    memset(result, 0, sizeof(*result)); result->stage = "DNS_TCP"; result->http_phase = -1;
    if (strcmp(request->url, "https://retroachievements.org/dorequest.php")) return -35308;
    if(dns_job.thread>=0)return -35424;
    memset(&address, 0, sizeof(address)); address.sin_family = AF_INET; address.sin_port = htons(443);
    STEP(IO_DNS_CREATE,sceNetResolverCreate(&network.resolver_id,dns_job.scratch,sizeof(dns_job.scratch)));
    STEP(IO_DNS_LOOKUP,ra_dns_run(network.resolver_id,&address.sin_addr,dns));
    STEP(IO_SOCKET,sceNetInetSocket(AF_INET, SOCK_STREAM, 0)); socket_fd = network.socket = rc;
    result->stage="socket_setup";
    STEP(IO_SOCKET_SETUP,ra_socket_nonblocking(socket_fd,socket_mode));
    result->stage="DNS_TCP";
    rc=transport_check();if(rc)goto end;
    io_enter(IO_CONNECT);
    rc = sceNetInetConnect(socket_fd, (struct sockaddr*)&address, sizeof(address));
    io_leave(IO_CONNECT);
    if (rc < 0) {
        error = sceNetInetGetErrno();
        if (error != EINPROGRESS && error != EALREADY && !retryable(error)) goto end;
        do {
            rc = transport_check(); if (rc) goto end;
            io_enter(IO_POLL);rc = ra_socket_poll(socket_fd,1,50);io_leave(IO_POLL);
            if (rc < 0) {if(sceNetInetGetErrno()==EINTR){rc=0;continue;}goto end;}
        } while (!rc);
        STEP(IO_CONNECT_ERROR,sceNetInetGetsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &error_size));
        if (error) { rc = MBEDTLS_ERR_NET_CONNECT_FAILED; goto end; }
    }
    memset(&options, 0, sizeof(options));
    options.send = send_bio; options.recv = recv_bio; options.wait = wait_io; options.entropy = tls_entropy;
    options.now_ms = now_ms; options.utc = utc; options.cancelled = transport_cancelled;
    options.ca_der = ra_tls_root; options.ca_size = sizeof(ra_tls_root); options.hostname = "retroachievements.org";
    options.post = request->post_data; options.post_size = request->post_size;
    options.body = response_body; options.capacity = RA_SESSION_RESPONSE_LIMIT; options.deadline_ms = transport_deadline;
    io_enter(IO_TLS);rc = ra_https_post(&options, result);io_leave(IO_TLS);
    if (!rc && result->heap_remaining) rc = -35309;
end:
    return rc;
#undef STEP
}
static int transport_release_io(void)
{
    int rc;
    if(dns_job.thread>=0) {
        rc=ra_dns_reap();if(rc)return rc<0?rc:-35424;
    }
    if (network.socket >= 0) {
        io_enter(IO_CLOSE);rc = sceNetInetClose(network.socket);io_leave(IO_CLOSE);if (rc < 0) return rc;
        network.socket = socket_fd = -1;
    }
    if (network.resolver_id >= 0) {
        io_enter(IO_DNS_DELETE);rc = sceNetResolverDelete(network.resolver_id);io_leave(IO_DNS_DELETE);if (rc < 0) return rc;
        network.resolver_id = -1;
    }
    return 0;
}
static int transport_retry(const transport_report* r)
{
    if(r->io_cleanup<0 || control->stop || r->https.verify_flags || r->code==-35308)return 0;
    return r->code==RA_HTTPS_TIMEOUT || (r->code==RA_HTTPS_CANCEL &&
        (now_ms(NULL)>=transport_deadline || __atomic_load_n(&sleep_events,__ATOMIC_ACQUIRE))) ||
        r->code==RA_HTTPS_TRUNCATED || r->code==MBEDTLS_ERR_NET_SEND_FAILED ||
        r->code==MBEDTLS_ERR_NET_RECV_FAILED || r->code==MBEDTLS_ERR_NET_CONNECT_FAILED ||
        r->code==MBEDTLS_ERR_SSL_CONN_EOF || r->code==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
        (r->code && r->https.stage && !strcmp(r->https.stage,"DNS_TCP"));
}
static int transport_worker(SceSize size, void* args)
{
    (void)size; (void)args;
    while (!transport_stopping && !control->stop) {
        ra_session_request request;
        transport_report finished;
        uint64_t started;
        if (!ra_mail_take(&mailbox, &request)) { sceKernelDelayThread(20000); continue; }
        memset(&finished, 0, sizeof(finished));
        finished.socket_mode.read_result=finished.dns.result=-35425;
        finished.ticket = request.ticket; finished.https.http_phase = -1;
        finished.kind=request.kind; started=now_ms(NULL);
        transport_ticket = request.ticket; transport_deadline = request.deadline_ms;
        mbedtls_platform_zeroize(response_body, RA_SESSION_RESPONSE_LIMIT);
        finished.code = transport_http(&request,&finished.https,&finished.socket_mode,&finished.dns);
        finished.io_cleanup = transport_release_io();
        finished.elapsed_ms=(unsigned)(now_ms(NULL)-started);
        if (finished.io_cleanup < 0 && !finished.code) finished.code = finished.io_cleanup;
        finished.retryable=transport_retry(&finished);
        /* Publish immutable response; owner copies it and delivers callbacks. */
        mailbox_finished = finished;
        if (!ra_mail_reply(&mailbox, finished.code ? (finished.retryable?RA_SESSION_RETRYABLE:-1) : finished.https.status,
                           finished.code ? 0 : finished.https.body_size)) {
            control->result = -35403; control->stop = 1;
        }
        if (finished.io_cleanup < 0) { control->result = finished.io_cleanup; control->stop = 1; }
    }
    /* Never allow stop_transport to acknowledge while DNS still owns module
     * memory or a resolver. On failure owner retains all dependencies. */
    while(dns_job.thread>=0) {
        if(!ra_dns_reap())break;
        sceKernelDelayThread(20000);
    }
    transport_done = 1;
    return 0;
}
#include "stop_transport.h"
static int read_auth(ra_auth* auth)
{
    unsigned char bytes[RA_AUTH_FILE_LIMIT + 1];
    int size, close_rc, ok;
    auth_fd = sceIoOpen(RA_CLIENT_DIR "auth.txt", PSP_O_RDONLY, 0);
    if (auth_fd < 0) return -35404;
    size = sceIoRead(auth_fd, bytes, sizeof(bytes)); close_rc = sceIoClose(auth_fd);
    if (close_rc >= 0) auth_fd = -1;
    ok = size > 0 && close_rc >= 0 && ra_auth_parse(bytes, (size_t)size, auth);
    ra_auth_erase(bytes, sizeof(bytes));
    return ok ? 0 : -35404;
}
static int read_mode(void)
{
    char bytes[16]; int n, rc, mode;
    SceUID fd = sceIoOpen(RA_CLIENT_DIR "mode.txt", PSP_O_RDONLY, 0);
    if (fd < 0) return 0; /* Missing marker always means spectator. */
    n = sceIoRead(fd, bytes, sizeof(bytes)); rc = sceIoClose(fd);
    if (n <= 0 || n == (int)sizeof(bytes) || rc < 0) return -35414;
    mode = ra_client_mode_parse(bytes, (unsigned)n);
    if (mode < 0) return -35414;
    online_mode = (unsigned)mode;
    return 0;
}
static int read_duration(void)
{
    char bytes[16]; unsigned seconds; int n, rc;
    SceUID fd=sceIoOpen(RA_CLIENT_DIR "duration.txt",PSP_O_RDONLY,0);
    if(fd<0)return 0; /* Missing file: whole session, independent of mode. */
    n=sceIoRead(fd,bytes,sizeof(bytes));rc=sceIoClose(fd);
    if(n<=0 || n==(int)sizeof(bytes) || rc<0 || !ra_client_duration_parse(bytes,(unsigned)n,&seconds))return -35416;
    sample_ms=seconds*1000u;return 0;
}
static int read_encore(void)
{
    char bytes[8];int n,rc,value;
    SceUID fd=sceIoOpen(RA_CLIENT_DIR "encore.txt",PSP_O_RDONLY,0);
    if(fd<0)return 0;
    n=sceIoRead(fd,bytes,sizeof(bytes));rc=sceIoClose(fd);
    value=n>0 && n<(int)sizeof(bytes) && rc>=0?ra_client_encore_parse(bytes,(unsigned)n):-1;
    if(value<0 || (value && !online_mode))return -35421;
    encore_mode=(unsigned)value;return 0;
}
static uint32_t read_game_segment(uint32_t address, uint8_t* bytes, uint32_t size, void* unused)
{
    const volatile unsigned char* source = (const volatile unsigned char*)(uintptr_t)address;
    uint32_t i;
    (void)unused;
    if (!control || control->stop || address < UINT32_C(0x08804000) || address >= UINT32_C(0x093FB580) ||
        !size || size > UINT32_C(0x093FB580)-address || !ra_game_map_contains(&game_reader.map, address, size)) return 0;
    for (i = 0; i < size; ++i) bytes[i] = source[i];
    return size;
}
static uint32_t session_read_game(uint32_t address, uint8_t* bytes, uint32_t size, void* unused)
{
    (void)unused;
    if (!control || control->stop) return 0;
    return ra_game_memory_read(address, bytes, size, &game_reader);
}
#include "resume_owner.h"
static void memory_read_report(void)
{
    unsigned i;
    ra_probe_log_printf(&logger, "GAME_READ calls=%u accepted=%u rejected=%u failed=%u tracked=%u denied_samples=%u scope=MODULE_SEGMENTS_ONLY\n",
        game_reader.calls, game_reader.accepted, game_reader.rejected, game_reader.failed,
        game_reader.sample_count, game_reader.denied_count);
    for (i = 0; i < game_reader.sample_count; ++i)
        ra_probe_log_printf(&logger, "GAME_READ_ACCEPT ra=0x%08X size=%u\n", game_reader.samples[i].address, game_reader.samples[i].size);
    for (i = 0; i < game_reader.denied_count; ++i)
        ra_probe_log_printf(&logger, "GAME_READ_DENY ra=0x%08X size=%u\n", game_reader.denied[i].address, game_reader.denied[i].size);
}
static void diagnostic_report(void)
{
    const ra_session_diagnostic* d=ra_session_get_diagnostic(runtime);
    const ra_session_warning* warning=ra_session_get_warning(runtime);
    unsigned i,j,start;
    if(warning) {
        ra_probe_log_printf(&logger,"CLIENT_WARNING id=%u title=%s\n",warning->id,warning->title);
        ra_probe_log_printf(&logger,"CLIENT_WARNING_DESCRIPTION %s\n",warning->description);
    }
    if(!d) { ra_probe_log_printf(&logger,"TARGET_DIAG unavailable=1\n"); return; }
    ra_probe_log_printf(&logger,"TARGET_DIAG id=%u conditions=%u refs=%u incomplete=%u changes=%u overwritten=%u\n",
        d->id,d->conditions,d->refs,d->incomplete,d->samples,d->overwritten);
    for(i=0;i<d->conditions;++i) {
        const ra_diag_condition* c=&d->condition[i];
        ra_probe_log_printf(&logger,"TARGET_COND n=%u group=%u type=%u op=%u required=%u true_frames=%u hit_frames=%u last_hits=%u max_hits=%u left=%u/%u/%u/%08X right=%u/%u/%u/%08X\n",
            i,c->group,c->type,c->oper,c->required_hits,c->true_frames,c->hit_frames,c->last_hits,c->max_hits,
            c->left.type,c->left.size,c->left.access,c->left.value,
            c->right.type,c->right.size,c->right.access,c->right.value);
    }
    for(i=0;i<d->refs;++i)
        ra_probe_log_printf(&logger,"TARGET_REF n=%u ra_address=%08X size=%u\n",i,d->addresses[i],d->sizes[i]);
    start=d->samples>RA_DIAG_SAMPLES?d->samples-RA_DIAG_SAMPLES:0;
    /* Include final snapshot even when it did not change on the final frame. */
    for(i=start;i<=d->samples;++i) {
        const ra_diag_sample* v=i==d->samples?&d->last:&d->sample[i%RA_DIAG_SAMPLES];
        ra_probe_log_printf(&logger,"TARGET_SAMPLE frame=%u time_ms=%u state=%u truths=%08X hits=%08X paused=%08X changed=%08X final=%u\n",
            v->frame,v->time_ms,v->state,v->truths,v->hits,v->paused_groups,v->changed_refs,i==d->samples);
        for(j=0;j<d->refs;++j)
            ra_probe_log_printf(&logger,"TARGET_VALUE n=%u current=%08X prior=%08X\n",j,v->value[j],v->prior[j]);
    }
}
static int observe_game(void)
{
    uint64_t until = now_ms(NULL) + sample_ms, next = 0;
    ra_session_stats stats;
    int observation_result = 0;
    engine_last_us = sceKernelGetSystemTimeLow();
    __sync_synchronize(); engine_enabled = 1;
    ra_probe_log_printf(&logger,"ONLINE_OBSERVE mode=%s duration_ms=%u time_ms=%u binding=CANDIDATE no_catchup=1\n",
        online_mode ? "SOFTCORE" : "SPECTATOR", sample_ms,(unsigned)now_ms(NULL));
    deadline = sample_ms ? until + 30000 : UINT64_MAX;
    while (!sample_ms || now_ms(NULL) < until) {
        int rc = check(); if (rc) { observation_result = engine_fault ? -35413 : rc; break; }
        /* Status logging does not take ownership of rc_client while callbacks
         * run. Otherwise a harmless log snapshot could make a game callback
         * fail admission. Detailed client events are drained after engine_stop. */
        if (now_ms(NULL) >= next && !__sync_lock_test_and_set(&timeline_lock, 1)) {
            ra_frame_timeline copy = timeline;
            __sync_lock_release(&timeline_lock);
            ra_probe_log_printf(&logger,"FRAME_STATUS frames=%u captured=%u missed=%u duplicate=%u skipped=%u frame_max_us=%u stack_min=%u fault=%u\n",
                (unsigned)control->frame_calls,copy.calls,(unsigned)timeline_missed,
                copy.duplicate_vcount,copy.skipped_vcount,engine_duration_max,
                engine_stack_min,(unsigned)engine_fault);
            ui_notice_status();
            next = now_ms(NULL) + 60000;
        }
        ui_poll();
        sceKernelDelayThread(5000);
    }
    engine_stop();
    ra_probe_log_printf(&logger,"UI_STATUS rows=%u popup_dropped=%u surface_failures=%u\n",
        online_ui?online_ui->count:0,online_ui?online_ui->dropped:0,ui_surface_failures);
    /* Give completed unlock requests up to 25s to finish. Idle never evaluates
     * current memory as a missed frame. No persistent retry queue is claimed. */
    until = now_ms(NULL) + 25000;
    do {
        ra_session_get_stats(runtime, &stats);
        if (!stats.pending && (!online_mode || stats.earned_events <= stats.award_confirmed)) break;
        if (check()) break;
        ui_poll();
        sceKernelDelayThread(20000);
    } while (now_ms(NULL) < until);
    if(online_ui) {
        ra_ui_notice notice;
        while(ra_ui_take(online_ui,&notice))ui_log_notice(&notice);
    }
    {
        ra_session_event event;
        while (ra_session_take_event(runtime, &event))
            ra_probe_log_printf(&logger,"ACHIEVEMENT_EVENT id=%u state=%s spectator=%u time_ms=%u\n",event.achievement_id,
                event.type==RA_SESSION_EVENT_CONDITION ? "CONDITION_MET" :
                event.type==RA_SESSION_EVENT_WARNING ? "CLIENT_WARNING" :
                event.type==RA_SESSION_EVENT_CONFIRMED ? "SERVER_CONFIRMED" : "SERVER_REJECTED",!online_mode,event.time_ms);
    }
    memory_read_report();
    diagnostic_report();
    ra_session_get_stats(runtime, &stats);
    ra_probe_log_printf(&logger,"ONLINE_DONE frames=%u earned=%u confirmed=%u pending=%u requests=%u errors=%u fault=%u last_confirmed=%u event_dropped=%u warnings=%u encore=%u\n",
        stats.frame_calls,stats.earned_events,stats.award_confirmed,stats.pending,
        stats.award_requests,stats.server_errors,(unsigned)engine_fault,stats.last_confirmed_id,stats.event_dropped,stats.warning_events,stats.encore_events);
    return observation_result ? observation_result : engine_fault ? -35413 : stats.frame_calls ? 0 : -35415;
}
static int startup_delay(const char* phase,unsigned* failures,const ra_session_stats* stats)
{
    uint64_t until;unsigned delay;
    if(!ra_startup_retry(stats->last_result,stats->last_http_status)) {
        ra_probe_log_printf(&logger,"RA_STARTUP_FAILED phase=%s result=%d http=%d retry=NO\n",phase,stats->last_result,stats->last_http_status);
        if(!ui_network_pending || !ui_network_reason)
            ui_network_error(12,0,!strcmp(phase,"LOGIN")?RA_REASON_AUTH:RA_REASON_SET,stats->last_result);
        return -35422;
    }
    delay=ra_retry_delay(*failures);if(*failures<6)++*failures;
    ra_probe_log_printf(&logger,"RA_RETRY phase=%s attempt=%u result=%d http=%d delay_ms=%u\n",phase,*failures+1,stats->last_result,stats->last_http_status,delay);
    if(ui_network_pending && ui_network_reason)ui_network_delay=(delay+999)/1000;
    else ui_network_error(!strcmp(phase,"LOGIN")?10:11,delay,
        !strcmp(phase,"LOGIN")?RA_REASON_AUTH:RA_REASON_SET,stats->last_result);
    until=now_ms(NULL)+delay;
    while(now_ms(NULL)<until){int rc=check();if(rc)return rc;sceKernelDelayThread(20000);}
    return 0;
}
static int run_session(int wifi, const char* hash)
{
    ra_auth auth;
    ra_session_stats stats;
    int rc;unsigned failures=0,attempt=0;
    memset(&auth, 0, sizeof(auth));
    deadline=UINT64_MAX;
    rc=ui_prepare();record("prepare startup UI",rc);if(rc)goto done;
    rc=wifi_begin(wifi);record("WIFI_BEGIN",rc);if(rc)goto done;
login_again:
    while(!__atomic_load_n(&wifi_online,__ATOMIC_ACQUIRE)) {
        rc=check();if(rc)goto done;sceKernelDelayThread(20000);
    }
    rc = read_auth(&auth); record("read session token (values hidden)", rc);
    if (rc) goto done;
    ra_probe_log_printf(&logger,"RA_ATTEMPT phase=LOGIN attempt=%u\n",++attempt);
    (void)ra_session_login_token(runtime,auth.username,auth.token);
    if(pending_enabled && mbedtls_sha256((const unsigned char*)auth.token,strlen(auth.token),pending_key,0)) {
        ra_auth_erase(&auth,sizeof(auth));rc=-35430;goto done;
    }
    ra_auth_erase(&auth,sizeof(auth));
    do {
        rc = check(); if (rc) goto done;
        ra_session_get_stats(runtime, &stats);
        if (!stats.auth_pending) break;
        startup_progress("LOGIN",&stats);
        sceKernelDelayThread(20000);
    } while (1);
    ra_probe_log_printf(&logger, "SESSION_LOGIN result=%d logged_in=%d spectator=%d hardcore=%d\n",
                        stats.last_result, stats.logged_in, stats.spectator, stats.hardcore);
    if(stats.last_result || !stats.logged_in) {
        rc=startup_delay("LOGIN",&failures,&stats);if(rc)goto done;goto login_again;
    }
    if(stats.spectator==(int)online_mode || stats.hardcore){rc=-35405;goto done;}
    failures=attempt=0;
load_again:
    while(!__atomic_load_n(&wifi_online,__ATOMIC_ACQUIRE)) {
        rc=check();if(rc)goto done;sceKernelDelayThread(20000);
    }
    ra_probe_log_printf(&logger,"RA_ATTEMPT phase=LOAD attempt=%u\n",++attempt);
    (void)ra_session_load_game(runtime,hash);
    do {
        rc = check(); if (rc) goto done;
        ra_session_get_stats(runtime, &stats);
        if (!stats.load_pending) break;
        startup_progress("LOAD",&stats);
        sceKernelDelayThread(20000);
    } while (1);
    drain_request_trace();
    if(stats.last_result || !stats.game_loaded) {
        rc=startup_delay("LOAD",&failures,&stats);if(rc)goto done;goto load_again;
    }
    memory_mark("spectator_set", "loaded");
    heap_mark("spectator_set");
    ra_probe_log_printf(&logger, "SPECTATOR_SET result=%d loaded=%d game_id=%u console_id=%u achievements=%u unsupported=%u spectator=%d hardcore=%d frames=%u earned=%u\n",
        stats.last_result, stats.game_loaded, stats.game_id, stats.console_id, stats.achievements,
        stats.unsupported, stats.spectator, stats.hardcore, stats.frame_calls, stats.earned_events);
    if (stats.last_result || !stats.game_loaded || stats.game_id != 10523 || stats.console_id != 41 ||
        !stats.achievements || stats.achievements > 128 || stats.spectator == (int)online_mode || stats.hardcore || stats.frame_calls || stats.earned_events) rc = -35406;
    else {
        ra_probe_log_printf(&logger,"SERVER_PROGRESS unlocked=%u target=397986 target_state=%d spectator=%u\n",
            stats.unlocked,ra_session_achievement_state(runtime,397986),!online_mode);
        memory_read_report();
        if(sample_ms)ra_probe_log_printf(&logger,"TARGET_DIAG_ENABLE id=397986 result=%d\n",ra_session_enable_diagnostic(runtime,397986));
        ui_network_pending=0;
        if(!pending_load()){rc=-35430;goto done;}
        rc = ra_session_prepare_ui(runtime,online_ui)?0:-35417;
        if(!rc)ui_badges_load();
        ui_render_valid=0;
        ui_notice=(ra_ui_notice){5,0,0};ui_notice_until=now_ms(NULL)+3500;++ui_notice_serial;
        record("prepare UI",rc);heap_mark("ui_prepare");
        if(!rc){log_stage("catalog_ready",0);rc = observe_game();}
    }
done:
    ra_auth_erase(&auth, sizeof(auth));
    if(rc && !control->stop && ui_ready) {
        uint64_t until=now_ms(NULL)+4000;
        ui_notice_until=now_ms(NULL);
        if(!ui_network_pending)ui_network_error(12,0,
            rc==-35404?RA_REASON_AUTH:rc==-35430?RA_REASON_QUEUE:rc==-35433?RA_REASON_RESUME:
            rc==-35305 || rc==-35417?RA_REASON_MEMORY:RA_REASON_APCTL,rc);
        while(now_ms(NULL)<until){if(check())break;sceKernelDelayThread(20000);}
    }
    return rc;
}
#if RA_CLIENT_RUNTIME_PROBE_ONLY
static int worker(SceSize size, void* args)
{
    int rc,close_rc;
    (void)size;(void)args;
    log_fd=sceIoOpen(RA_CLIENT_DIR "client-user.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0666);
    if(log_fd<0){control->result=log_fd;control->done=1;return 0;}
    ra_probe_log_init(&logger,log_write,NULL);
    log_session=control->log_session;
    ra_probe_log_printf(&logger,"LOG_SESSION id=%u component=user\n",(unsigned)log_session);
    log_stage("user_worker_enter",0);
    ra_probe_log_printf(&logger,"client_version=%s input method runtime probe; frames=DISABLED no_award=1\n",RA_CLIENT_VERSION);
    rc=run_runtime_probe();
    if(logger.error && !rc)rc=logger.error;
    ra_probe_log_printf(&logger,"client_finished result=%d cleanup_result=0 log_error=0x%08X\n",
        rc,(unsigned)logger.error);
    close_rc=sceIoClose(log_fd);if(close_rc>=0)log_fd=-1;else if(!rc)rc=close_rc;
    control->result=rc;control->done=1;return 0;
}
#else
static int worker(SceSize size, void* args)
{
    char* argv[] = { RA_CLIENT_DIR "RAClientUser.prx", NULL };
    char hash[33];
    int rc = 0, cleanup_error = 0, wifi, libc_ready = 0;
    const ra_session_config session_config = { NULL, now_ms, session_read_game, queue_lock, queue_unlock };
    static const unsigned char parent_label[] = "RAClientPSP-parent/0.16";
    const ScePspDateTime epoch = {1970, 1, 1, 0, 0, 0, 0};
    (void)size; (void)args;
    ra_net_resources_init(&network);
    ra_probe_power_init(&power_owner);
    mbedtls_ctr_drbg_init(&entropy_rng);
    log_fd = sceIoOpen(RA_CLIENT_DIR "client-user.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0) { rc = log_fd; goto done; }
    ra_probe_log_init(&logger, log_write, NULL);
    log_session=control->log_session;
    ra_probe_log_printf(&logger,"LOG_SESSION id=%u component=user\n",(unsigned)log_session);
    log_stage("user_worker_enter",0);
    log_sync_next=now_ms(NULL)+5000u;
    ra_probe_log_printf(&logger, "client_version=%s online client; mode_marker=REQUIRED_FOR_AWARDS\n", RA_CLIENT_VERSION);
#if RA_CLIENT_RUNTIME_PROBE_ONLY
    rc=run_runtime_probe();goto done;
#endif
    memory_mark("libc_init", "before");
    log_stage("libc_init_begin",0);
    __libcglue_init(1, argv); libc_ready = 1;
    log_stage("libc_init_end",0);
    memory_mark("libc_init", "after");
    memory_mark("rc_client_create", "before");
    queue_guard = sceKernelCreateSema("RASessionQueue", 0, 1, 1, NULL);
    if (queue_guard < 0) { rc = queue_guard; goto done; }
    runtime = ra_session_create(&session_config);
    response_body = malloc(RA_SESSION_RESPONSE_LIMIT);
    if (!runtime || !response_body) { rc = -35315; goto done; }
    rc = read_mode(); if (rc) goto done;
    rc = read_encore(); if (rc) goto done;
    rc = read_duration(); if (rc) goto done;
    ra_probe_log_printf(&logger,"OBSERVATION_CONFIG duration_ms=%u sync_period_ms=5000\n",sample_ms);
    if (online_mode && !ra_session_enable_online_softcore(runtime)) { rc = -35414; goto done; }
    if(encore_mode && !ra_session_enable_encore(runtime)){rc=-35421;goto done;}
    ra_probe_log_printf(&logger,"ENCORE_CONFIG enabled=%u known_awards=LOCAL_ONLY\n",encore_mode);
    ra_probe_log_printf(&logger,"CLIENT_MODE spectator=%u hardcore=0 online_awards=%u\n",!online_mode,online_mode);
    control->probe_wrapper = (uint32_t)(uintptr_t)&ra_runtime_input_probe;
    control->notice_wrapper = (uint32_t)(uintptr_t)&ra_notice_present;
    __sync_synchronize();
    memory_mark("rc_client_create", "after");
    ra_probe_log_printf(&logger, "RC_CLIENT_TLS_READY spectator=%u hardcore=0 game_loaded=0 frames=WAITING heap=%u response_limit=%u\n",
                        !online_mode, RA_CLIENT_HEAP_BYTES, RA_SESSION_RESPONSE_LIMIT);
    rc = record("register power callback", ra_probe_power_register(&power_owner, &power_api)); if (rc < 0) goto done;
    rc = record("RTC anchor", sceRtcGetTick(&epoch, &epoch_tick)); if (rc < 0) goto done;
    resolution = sceRtcGetTickResolution(); if (!resolution || utc(NULL) == (time_t)-1) { rc = -35310; goto done; }
    if(online_mode) {
        if(!ra_session_pending_enable(runtime,(uint64_t)utc(NULL))){rc=-35430;goto done;}
        pending_enabled=1;
    }
    /* A KIRK-seeded parent DRBG supplies independent entropy for each bounded
     * TLS connection. Never reuse the 0.15 one-connection seed bytes. */
    rc = mbedtls_ctr_drbg_seed(&entropy_rng, entropy, NULL, parent_label, sizeof(parent_label) - 1);
    if (rc) goto done;
    wifi = record("read explicit WiFi profile", profile()); if (wifi < 0) { rc = wifi; goto done; }
    deadline = now_ms(NULL) + 20000;
    log_stage("early_network_begin",0);
    rc = prepare_early_network(); if (rc) goto done;
    log_stage("early_network_end",0);
    transport_thread = sceKernelCreateThread("RASessionHTTP", transport_worker, 0x60, 32768, PSP_THREAD_ATTR_USER, NULL);
    if (transport_thread < 0) { rc = transport_thread; goto done; }
    rc = sceKernelStartThread(transport_thread, 0, NULL);
    if (rc < 0) goto done;
    transport_started = 1;
    ra_probe_log_printf(&logger, "EARLY_NETWORK_READY user_free=%u largest=%u request=WAITING connect=DEFERRED\n",
        (unsigned)sceKernelTotalFreeMemSize(), (unsigned)sceKernelMaxFreeMemSize());
    heap_mark("early_ready");
    log_stage("request_release_wait",0);
    rc = wait_request_release(); if (rc) goto done;
    log_stage("request_release_received",0);
    if (RA_CLIENT_STATIC_AUDIT_ONLY) {
        ra_probe_log_printf(&logger,"STATIC_AUDIT_ONLY complete=1 auth_read=0 requests=0 frames=0 binding=UNPROVEN\n");
        goto done;
    }
    __sync_synchronize();
    if (!ra_game_reader_init(&game_reader, &control->game_map, read_game_segment, NULL)) { rc = -35409; goto done; }
    if (game_reader.map.count != 1 || game_reader.map.regions[0].address != UINT32_C(0x08804000) ||
        game_reader.map.regions[0].size != UINT32_C(0x00BF7580)) { rc = -35409; goto done; }
    ra_probe_log_printf(&logger, "GAME_MEMORY_READY segments=%u ra_base=0x00800000 extra=DENIED frames=WAITING\n", game_reader.map.count);
    if(read_game_segment(0x08842D90u,resume_code,sizeof(resume_code),NULL)!=sizeof(resume_code)) {rc=-35433;goto done;}
    resume_code_ready=1;
    /* Network, module image, stack and libc heap remain owned during the
     * game's startup. Only now read the disc and start the HTTPS request. */
    deadline = now_ms(NULL) + 100000;
    log_stage("disc_hash_begin",0);
    rc = record("disc0 hash matches official PC result", disc_hash(hash)); if (rc) goto done;
    log_stage("disc_hash_end",0);
    ra_probe_log_printf(&logger, "HTTPS_READY heap=%u stack=%u user_free=%u largest=%u\n", RA_CLIENT_HEAP_BYTES,
        RA_CLIENT_STACK_BYTES, (unsigned)sceKernelTotalFreeMemSize(), (unsigned)sceKernelMaxFreeMemSize());
    log_stage("online_session_begin",0);
    rc = run_session(wifi, hash);
done:
    log_lifecycle=1;
    log_stage("user_cleanup_begin",rc);
    engine_stop();
    pending_capture();pending_service();
    wifi_enabled=0;__atomic_store_n(&wifi_online,0,__ATOMIC_RELEASE);
    ra_probe_log_printf(&logger,"STOP_DIAGNOSTIC module_stop=%u power_flags=0x%08X home_seen=%u exit_user_flags=%u\n",stop_from_module,power_stop_flags,ui_home_seen,(unsigned)control->exit_user_flags);
    log_stage("ui_shutdown_begin",0);
    cleanup_error = ui_shutdown();
    log_stage("ui_shutdown_end",cleanup_error);
    if(cleanup_error<0)goto finish;
    log_stage("transport_stop_begin",0);
    cleanup_error = stop_transport();
    log_stage("transport_stop_end",cleanup_error);
    if (log_fd >= 0) record("stop HTTPS transport", cleanup_error);
    if (cleanup_error < 0) goto finish; /* Retain every object a live thread may reference. */
    if (runtime && !ra_mail_cancel_joined(&mailbox, runtime)) { cleanup_error = -35403; goto finish; }
    drain_transport_report();
    if (auth_fd >= 0) {
        cleanup_error = sceIoClose(auth_fd);
        if (cleanup_error < 0) goto finish;
        auth_fd = -1;
    }
    if (runtime) {
        ra_session_stats stats;
        ra_session_get_stats(runtime, &stats);
        ra_probe_log_printf(&logger, "SESSION_QUEUE current=%u peak=%u limit=%u last_copy_error=%d pending=%u submitted=%u completed=%u\n",
            (unsigned)stats.buffer_bytes, (unsigned)stats.peak_buffer_bytes, RA_SESSION_BUFFER_LIMIT,
            stats.last_copy_error, stats.pending, stats.submitted, stats.completed);
        memory_mark("rc_client_logout_destroy", "before");
        if (!ra_session_close(runtime)) { cleanup_error = -35408; goto finish; }
        runtime = NULL;
        memory_mark("rc_client_logout_destroy", "after");
        heap_mark("after_client_destroy");
    }
    if (response_body) { mbedtls_platform_zeroize(response_body, RA_SESSION_RESPONSE_LIMIT); free(response_body); response_body = NULL; }
    mbedtls_ctr_drbg_free(&entropy_rng);
    mbedtls_platform_zeroize(pending_key,sizeof(pending_key));
    if (queue_guard >= 0) {
        cleanup_error = sceKernelDeleteSema(queue_guard);
        if (cleanup_error < 0) goto finish;
        queue_guard = -1;
    }
    mbedtls_platform_zeroize(control->entropy, sizeof(control->entropy));
    cleanup_error = ra_net_resources_release(&network, network_release, NULL);
    cleanup_error = release_apctl_observer(cleanup_error);
    if (!cleanup_error) cleanup_error = ra_probe_power_release(&power_owner, &power_api);
finish:
    log_stage("user_cleanup_end",cleanup_error);
    if (logger.error && !rc) rc = logger.error;
    if (control->result && (!rc || rc == RA_HTTPS_CANCEL)) rc = control->result;
    if (log_fd >= 0) {
        ra_probe_log_printf(&logger, "client_finished result=%d cleanup_result=%d log_error=0x%08X sync_calls=%u sync_max_ms=%u sync_last=0x%08X\n", rc, cleanup_error, (unsigned)logger.error,log_sync_calls,log_sync_max_ms,(unsigned)log_sync_last_result);
        log_stage("user_log_sync_close",cleanup_error < 0 ? cleanup_error : rc);
        int sync_rc=sync_logs(1); if(sync_rc<0 && !cleanup_error)cleanup_error=sync_rc;
        int close_rc = sceIoClose(log_fd); if (close_rc < 0) cleanup_error = close_rc; else log_fd = -1;
    }
    control->result = cleanup_error < 0 ? cleanup_error : rc;
    if (cleanup_error < 0) { control->stop = 1; return 0; }
    control->free_before_deinit = (unsigned)sceKernelTotalFreeMemSize();
    if (libc_ready) __libcglue_deinit();
    control->free_after_deinit = (unsigned)sceKernelTotalFreeMemSize();
    /* Preserve the request result above; successful teardown exits with zero. */
    control->done = 1; return 0;
}
#endif
int module_start(SceSize size, void* args)
{
    ra_client_args request; uintptr_t address; int rc;
    if (size != sizeof(request) || !args) return -35311;
    memcpy(&request, args, sizeof(request)); address = (uintptr_t)request.control;
    if (address > UINT32_MAX || address % 4 || !ra_client_user_range((uint32_t)address, sizeof(*control))) return -35311;
    control = request.control;
    if (control->magic != RA_CLIENT_MAGIC || control->size != sizeof(*control) || control->entropy_status) { control = NULL; return -35311; }
    thread_id = sceKernelCreateThread("RAHttpsOwner", worker, 0x60, RA_CLIENT_STACK_BYTES, PSP_THREAD_ATTR_USER, NULL);
    control->owner_thread_id = thread_id;
    if (thread_id < 0) return thread_id;
    rc = sceKernelStartThread(thread_id, 0, NULL);
    if (rc < 0 && sceKernelDeleteThread(thread_id) >= 0) thread_id = -1;
    return rc;
}
#include "stop_owner.h"
int module_stop(SceSize size, void* args)
{ (void)size; (void)args;__atomic_store_n(&stop_from_module,1,__ATOMIC_RELEASE);return https_stop_owner(); }
int module_reboot_before(SceSize size, void* args)
{ (void)size; (void)args; return https_reboot_before(); }
