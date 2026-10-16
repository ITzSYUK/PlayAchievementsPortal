#include <pspkernel.h>
#include <pspsdk.h>
#include <psploadcore.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <stdio.h>
#include <string.h>
#include "probe_log.h"
#include "probe_shared.h"
#include "probe_policy.h"
#include "ra_psp_memory.h"

PSP_MODULE_INFO("RAInGameProbe", PSP_MODULE_KERNEL, 0, 2);
static volatile int stopping;
static SceUID thread_id = -1, child = -1, shared_block = -1, fd = -1;
static ra_ingame_shared* shared;
static ra_probe_log logger;
static unsigned written;

static int log_write(const void* data, unsigned size, void* unused)
{
    int rc;
    (void)unused;
    if (size > RA_INGAME_LOG_LIMIT - written) return -34002;
    rc = sceIoWrite(fd, data, size);
    if (rc > 0) written += (unsigned)rc;
    return rc;
}
static int record(const char* name, int rc)
{
    ra_probe_log_printf(&logger, "%s: 0x%08X\n", name, (unsigned)rc);
    return rc;
}
static int foreign_network(void)
{
    SceUID ids[256];
    int count = 0, i;
    int rc = sceKernelGetModuleIdList(ids, sizeof(ids), &count);
    if (rc < 0 || count < 0 || count > 256) return 1;
    for (i = 0; i < count; ++i) {
        SceModule* module = sceKernelFindModuleByUID(ids[i]);
        if (module && (!strncmp(module->modname, "sceNet", 6) || !strncmp(module->modname, "sceHttp", 7))) {
            ra_probe_log_printf(&logger, "foreign_network_module=%.27s\n", module->modname);
            return 1;
        }
    }
    return 0;
}
static uint32_t fingerprint(const volatile unsigned char* bytes, unsigned length)
{
    uint32_t value = 2166136261u;
    while (length--) { value ^= *bytes++; value *= 16777619u; }
    return value;
}
typedef struct { ra_probe_region regions[4]; unsigned count; } read_context;
static uint32_t read_segment(uint32_t address, uint8_t* buffer, uint32_t length, void* userdata)
{
    const read_context* context = userdata;
    const volatile unsigned char* source;
    uint32_t i;
    if (!ra_probe_region_contains(context->regions, context->count, address, length)) return 0;
    source = (const volatile unsigned char*)address;
    for (i = 0; i < length; ++i) buffer[i] = source[i];
    return length;
}
static int sample_module(SceUID game, unsigned* checksums, unsigned* sample_count)
{
    SceKernelModuleInfo info;
    read_context context;
    unsigned char buffer[64];
    unsigned i;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    if (sceKernelQueryModuleInfo(game, &info) < 0 || strcmp(info.name, RA_INGAME_MODULE) ||
        (info.attribute & PSP_MODULE_KERNEL) || info.nsegment < 1 || info.nsegment > 4) return -1;
    context.count = (unsigned)info.nsegment;
    for (i = 0; i < context.count; ++i) {
        context.regions[i].address = (uint32_t)info.segmentaddr[i]; context.regions[i].size = (uint32_t)info.segmentsize[i];
    }
    *sample_count = 0;
    for (i = 0; i < (unsigned)info.nsegment; ++i) {
        const uint32_t address = context.regions[i].address;
        unsigned length = context.regions[i].size < 64 ? context.regions[i].size : 64;
        if (!length) continue;
        if (!ra_probe_region_contains(context.regions, context.count, address, length)) return -1;
        /* PSP user RAM is physically accessible from this kernel thread.
         * No arbitrary addresses, dumps, writes or kernel RAM are sampled.
         * Snapshot may change while the game runs; it is not an atomic frame. */
        if (ra_psp_memory_read(address - UINT32_C(0x08000000), buffer, length,
            RA_PSP_RAM_32_MB, read_segment, &context) != length) return -1;
        checksums[i] = fingerprint(buffer, length);
        ++*sample_count;
    }
    return *sample_count ? 0 : -1;
}
static int target_game(void)
{
    char id[11] = {0};
    SceUID file = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0);
    int rc;
    if (file < 0) return file;
    rc = sceIoRead(file, id, 10); sceIoClose(file);
    return rc == 10 && !strcmp(id, RA_INGAME_UMD) ? 0 : -34003;
}
static void stop_child(void)
{
    int status = -1, rc;
    if (shared) shared->stop = 1;
    if (child < 0) return;
    rc = record("stop user worker", sceKernelStopModule(child, 0, NULL, &status, NULL));
    record("worker stop status", status);
    if (rc < 0 || status != 0) {
        /* Refuse unsafe unload. Live code and its shared allocation stay valid. */
        ra_probe_log_printf(&logger, "worker still active: code/shared block retained until process exit\n");
        return;
    }
    rc = record("unload user worker", sceKernelUnloadModule(child));
    if (rc >= 0) child = -1;
}
static int observer(SceSize args, void* argp)
{
    SceModule* module = NULL;
    SceUID game = -1;
    unsigned tries, previous_buttons = 0, phase = 0, previous_vcount, flips = 0, samples = 0;
    void* previous_buffer = NULL;
    const char* phases[] = {"menu", "gameplay", "paused-manual", "resumed-manual"};
    SceUInt64 started, reported, last_poll;
    unsigned previous_checksums[4] = {0};
    unsigned previous_count = 0;
    int have_memory = 0;
    (void)args; (void)argp;
    fd = sceIoOpen(RA_INGAME_DIR "bootstrap.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (fd < 0) return 1;
    ra_probe_log_init(&logger, log_write, NULL);
    ra_probe_log_printf(&logger, "probe_version=0.2 kernel bootstrap target=%s module=%s\n", RA_INGAME_UMD, RA_INGAME_MODULE);
    ra_probe_log_printf(&logger, "No hooks, game writes, rc_client frames, credentials or unlocks.\n");
    for (tries = 0; tries < 120 && !stopping; ++tries) {
        if (target_game() == 0 && (module = sceKernelFindModuleByName(RA_INGAME_MODULE)) != NULL) break;
        sceKernelDelayThread(250000);
    }
    if (!module || stopping) { record("target game unavailable (30s)", -34003); goto cleanup; }
    game = module->modid;
    ra_probe_log_printf(&logger, "game_uid=0x%08X text=0x%08X text_size=%u segments=%u\n",
        (unsigned)game, (unsigned)module->text_addr, (unsigned)module->text_size, (unsigned)module->nsegment);
    shared_block = record("allocate user control", sceKernelAllocPartitionMemory(2, "RAProbeControl", PSP_SMEM_Low, sizeof(*shared), NULL));
    if (shared_block < 0) goto cleanup;
    shared = sceKernelGetBlockHeadAddr(shared_block);
    memset(shared, 0, sizeof(*shared));
    shared->magic = RA_INGAME_MAGIC; shared->version = 1; shared->size = sizeof(*shared);
    shared->network_present = 1;
    /* The public power API and its callback belong to the user worker.
     * The kernel driver call returned 0x80000004 on the owner's 6.60/ARK5. */
    ra_probe_log_printf(&logger, "power_callback_owner=user_worker\n");
    {
        SceKernelLMOption option;
        ra_ingame_args request = { shared };
        int status = -1;
        memset(&option, 0, sizeof(option)); option.size = sizeof(option);
        option.mpidtext = 2; option.mpiddata = 2; option.position = 0; option.access = 1;
        child = record("load user worker", sceKernelLoadModule(RA_INGAME_DIR "RAInGameWorker.prx", 0, &option));
        if (child < 0) goto cleanup;
        if (record("start user worker", sceKernelStartModule(child, sizeof(request), &request, &status, NULL)) < 0 || status != 0) {
            int unloaded;
            record("worker start status", status);
            unloaded = record("unload unstarted worker", sceKernelUnloadModule(child));
            if (unloaded >= 0) child = -1;
            goto cleanup;
        }
    }
    started = reported = last_poll = sceKernelGetSystemTimeWide();
    previous_vcount = sceDisplayGetVcount();
    ra_probe_log_printf(&logger, "observation_started duration_s=300 poll_us=5000\n");
    sceIoSync("ms0:", 0);
    while (!stopping && !shared->stop && sceKernelGetSystemTimeWide() - started < 300000000u) {
        void* buffer = NULL;
        int width, format;
        SceCtrlData pad;
        unsigned current_buttons;
        SceUInt64 now = sceKernelGetSystemTimeWide();
        if (now - last_poll > 2000000u) {
            ra_probe_log_printf(&logger, "observation_gap_ms=%u: sample window reset (not a frame count)\n", (unsigned)((now - last_poll) / 1000));
            reported = now; flips = samples = 0; previous_vcount = sceDisplayGetVcount();
        }
        last_poll = now;
        if (sceDisplayGetFrameBuf(&buffer, &width, &format, PSP_DISPLAY_SETBUF_IMMEDIATE) == 0) {
            if (previous_buffer && buffer != previous_buffer) ++flips;
            previous_buffer = buffer;
        }
        ++samples;
        if (sceCtrlPeekBufferPositive(&pad, 1) > 0) {
            const unsigned phase_keys = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT;
            const unsigned tls_keys = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SQUARE;
            current_buttons = pad.Buttons;
            if ((current_buttons & phase_keys) == phase_keys && (previous_buttons & phase_keys) != phase_keys) {
                phase = (phase + 1) % 4;
                ra_probe_log_printf(&logger, "manual_phase=%s elapsed_ms=%u\n", phases[phase], (unsigned)((now - started) / 1000));
                reported = now; flips = samples = 0; previous_vcount = sceDisplayGetVcount();
            }
            if ((current_buttons & tls_keys) == tls_keys && (previous_buttons & tls_keys) != tls_keys && !shared->tls_request) {
                shared->network_present = (unsigned)foreign_network();
                shared->tls_request = 1;
                ra_probe_log_printf(&logger, "HTTPS requested foreign_network=%u worker_ready=%u\n", (unsigned)shared->network_present, (unsigned)shared->ready);
            }
            previous_buttons = current_buttons;
        }
        if (now - reported >= 1000000u) {
            unsigned vcount = sceDisplayGetVcount(), checksums[4] = {0}, count = 0, changed = 0, i;
            int memory_result = sample_module(game, checksums, &count);
            if (memory_result < 0) { record("module segment read unavailable", memory_result); break; }
            for (i = 0; i < count; ++i) if (have_memory && (i >= previous_count || previous_checksums[i] != checksums[i])) ++changed;
            memcpy(previous_checksums, checksums, sizeof(checksums)); previous_count = count; have_memory = 1;
            ra_probe_log_printf(&logger, "sample t_ms=%u phase=%s window_ms=%u vblank=%u observed_buffer_changes=%u polls=%u memory_segments=%u changed_segments=%u TLS=%u/%u\n",
                (unsigned)((now - started) / 1000), phases[phase], (unsigned)((now - reported) / 1000),
                vcount - previous_vcount, flips, samples, count, changed, (unsigned)shared->tls_request, (unsigned)shared->tls_done);
            ra_probe_log_printf(&logger, "fingerprints=%08X,%08X,%08X,%08X (first <=64 bytes/segment; not game state or frame count)\n",
                checksums[0], checksums[1], checksums[2], checksums[3]);
            previous_vcount = vcount; flips = samples = 0; reported = now;
        }
        sceKernelDelayThreadCB(5000);
    }
cleanup:
    stop_child();
    if (shared) ra_probe_log_printf(&logger, "worker_ready=%u done=%u result=%d tls_done=%u tls_code=%d http=%d suspended=%u\n",
        (unsigned)shared->ready, (unsigned)shared->worker_done, shared->worker_result, (unsigned)shared->tls_done, shared->tls_result, shared->http_status, (unsigned)shared->suspended);
    if (shared_block >= 0 && child < 0) {
        int interrupt_state = sceKernelCpuSuspendIntr();
        /* Revoke the pointer before releasing the allocation. module_stop's
         * stop write uses the same short nonblocking critical section. */
        shared = NULL;
        sceKernelCpuResumeIntr(interrupt_state);
        record("free user control", sceKernelFreePartitionMemory(shared_block)); shared_block = -1;
    }
    ra_probe_log_printf(&logger, "observation_finished; frame_binding=UNPROVEN; log_error=0x%08X\n", (unsigned)logger.error);
    sceIoClose(fd); fd = -1; sceIoSync("ms0:", 0);
    return 0;
}
int module_start(SceSize args, void* argp)
{
    int rc;
    (void)args; (void)argp;
    thread_id = sceKernelCreateThread("RAProbeObserver", observer, 0x38, 0x6000, 0, NULL);
    if (thread_id < 0) return thread_id;
    rc = sceKernelStartThread(thread_id, 0, NULL);
    if (rc < 0) { sceKernelDeleteThread(thread_id); thread_id = -1; }
    return rc;
}
int module_stop(SceSize args, void* argp)
{
    SceUInt timeout = 12000000;
    int rc;
    int interrupt_state;
    (void)args; (void)argp; stopping = 1;
    interrupt_state = sceKernelCpuSuspendIntr();
    if (shared) shared->stop = 1;
    sceKernelCpuResumeIntr(interrupt_state);
    if (thread_id < 0) return 0;
    rc = sceKernelWaitThreadEnd(thread_id, &timeout);
    if (rc < 0 || child >= 0) return -1;
    sceKernelDeleteThread(thread_id); thread_id = -1;
    return 0;
}
