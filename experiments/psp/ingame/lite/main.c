#ifdef RA_LITE_HOST_TEST
#include "lite_test_sdk.h"
#else
#include <pspkernel.h>
#include <pspkerror.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#endif
#include <string.h>
#include "probe_log.h"
#include "extra_ram_policy.h"
#include "placement2_policy.h"

/* Stability baseline after the full 0.2 probe powered the owner's PSP off.
 * Only a tiny unstarted child is loaded in partition 2. No worker, newlib heap,
 * callbacks, disc reads, pad/display APIs, network, or game segment reads. */
PSP_MODULE_INFO("RAInGameLite", PSP_MODULE_KERNEL, 0, 8);
#define LOG_PATH "ms0:/seplugins/RAInGameLite/placement-user2.log"
#define LOG_LIMIT 32768u
static volatile int stopping;
static SceUID observer_thread = -1, log_fd = -1;
static ra_probe_log logger;
static unsigned written;
static SceUID reserve_id = -1;
static volatile unsigned char* reserve_pointer;
static SceUID placement_module = -1;

static int log_write(const void* data, unsigned size, void* unused)
{
    int rc;
    (void)unused;
    if (size > LOG_LIMIT - written) return -34002;
    rc = sceIoWrite(log_fd, data, size);
    if (rc > 0) written += (unsigned)rc;
    return rc;
}
static int record(const char* name, int rc)
{
    ra_probe_log_printf(&logger, "%s: 0x%08X\n", name, (unsigned)rc);
    return rc;
}
static const char* result_name(int rc)
{
    if (!rc) return "NONE";
    if (rc == 1) return "RESERVE_SKIPPED";
    if (rc == RA_EXTRA_PLACEMENT_UNSUPPORTED) return "PLACEMENT_UNSUPPORTED";
    if (rc == RA_PLACEMENT2_MEMORY_GUARD) return "USER_MEMORY_GUARD";
    if ((unsigned)rc == (unsigned)SCE_KERNEL_ERROR_PARTITION_MISMATCH) return "PARTITION_MISMATCH";
    if (rc == -34102) return "INVALID_EXTRA_RANGE";
    if (rc == -34103) return "OWNED_BLOCK_MISMATCH";
    if (rc == -34104) return "INVALID_MODULE_METADATA";
    if (rc == -34105) return "INVALID_MEMORY_SNAPSHOT";
    if (rc == -34001) return "STOP_OR_LOG_ERROR";
    if (rc == -34003) return "TARGET_UNAVAILABLE_OR_STOPPED";
    return "OTHER_ERROR";
}
static int sample_memory(unsigned second, unsigned* minimum_total, unsigned* minimum_max)
{
    PspSysmemPartitionInfo partition;
    unsigned total, largest;
    int rc;
    memset(&partition, 0, sizeof(partition));
    partition.size = sizeof(partition);
    /* Log the first call boundaries to distinguish API failures from a later
     * game crash. A free-memory sample is not a reservation or an OOM proof. */
    if (!second) ra_probe_log_printf(&logger, "begin: query user partition 2\n");
    rc = sceKernelQueryMemoryPartitionInfo(2, &partition);
    if (rc < 0) { record("user partition unavailable", rc); return -1; }
    if (!second) ra_probe_log_printf(&logger, "user_partition start=0x%08X size=%u\n",
        (unsigned)partition.startaddr, (unsigned)partition.memsize);
    if (!second) ra_probe_log_printf(&logger, "begin: total free user memory\n");
    total = (unsigned)sceKernelPartitionTotalFreeMemSize(2);
    if (!second) ra_probe_log_printf(&logger, "begin: largest free user block\n");
    largest = (unsigned)sceKernelPartitionMaxFreeMemSize(2);
    if (!partition.memsize || total > partition.memsize || largest > total) {
        ra_probe_log_printf(&logger, "MEMORY_QUERY_INVALID total=0x%08X max=0x%08X partition_size=%u\n",
            total, largest, (unsigned)partition.memsize);
        return -1;
    }
    if (!second || total < *minimum_total) *minimum_total = total;
    if (!second || largest < *minimum_max) *minimum_max = largest;
    ra_probe_log_printf(&logger, "sample second=%u user_free=%u user_largest=%u minimum_free=%u minimum_largest=%u\n",
        second, total, largest, *minimum_total, *minimum_max);
    if (placement_module >= 0 && !ra_placement2_can_hold(partition.memsize, total, largest)) {
        ra_probe_log_printf(&logger, "USER_MEMORY_GUARD phase=observe free=%u largest=%u floor=%u; end observation and unload\n",
            total, largest, (unsigned)RA_PLACEMENT2_FREE_FLOOR);
        return logger.error ? -1 : RA_PLACEMENT2_MEMORY_GUARD;
    }
    return logger.error ? -1 : 0;
}
static void sample_layout(unsigned second)
{
    /* ARK5 reference sysmem.c considers partitions 8..11 for extra RAM.
     * 1/2/5 are reported for context. Neither attributes nor free space
     * demonstrate that a user worker may execute/allocate there safely. */
    const int partitions[] = {1, 2, 5, 8, 9, 10, 11};
    unsigned i;
    ra_probe_log_printf(&logger, "layout_begin second=%u partition9_reserved=%u permission_changes=NONE\n",
        second, reserve_id >= 0 ? (unsigned)RA_EXTRA_RESERVE_BYTES : 0);
    for (i = 0; i < sizeof(partitions) / sizeof(partitions[0]) && !stopping; ++i) {
        PspSysmemPartitionInfo info;
        unsigned total, largest;
        int rc, pid = partitions[i];
        memset(&info, 0, sizeof(info)); info.size = sizeof(info);
        ra_probe_log_printf(&logger, "begin: query partition %d\n", pid);
        rc = sceKernelQueryMemoryPartitionInfo(pid, &info);
        if (rc < 0) {
            ra_probe_log_printf(&logger, "partition=%d unavailable code=0x%08X (no allocation attempted)\n", pid, (unsigned)rc);
            continue; /* Missing optional partitions are an expected result. */
        }
        total = (unsigned)sceKernelPartitionTotalFreeMemSize(pid);
        largest = (unsigned)sceKernelPartitionMaxFreeMemSize(pid);
        ra_probe_log_printf(&logger, "partition=%d start=0x%08X size=%u attr=0x%08X free=%u largest=%u valid=%u\n",
            pid, (unsigned)info.startaddr, (unsigned)info.memsize, (unsigned)info.attr,
            total, largest, (unsigned)(info.memsize && total <= info.memsize && largest <= total));
    }
    ra_probe_log_printf(&logger, "layout_end second=%u user_access=UNPROVEN\n", second);
}
static unsigned char pattern(unsigned offset)
{ return (unsigned char)(offset ^ (offset >> 8) ^ 0xA5u); }
static int check_owned_block(int initialize)
{
    const unsigned offsets[] = {0, RA_EXTRA_RESERVE_BYTES / 2, RA_EXTRA_RESERVE_BYTES - 128};
    unsigned i, j;
    if (!reserve_pointer || reserve_id < 0) return -34102;
    for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        if (!ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, offsets[i], 128)) return -34102;
        for (j = 0; j < 128; ++j) {
            const unsigned offset = offsets[i] + j;
            if (initialize) reserve_pointer[offset] = pattern(offset);
            if (reserve_pointer[offset] != pattern(offset)) {
                ra_probe_log_printf(&logger, "OWNED_BLOCK_MISMATCH offset=%u\n", offset);
                return -34103;
            }
        }
    }
    return 0;
}
static int reserve_extra(void)
{
    PspSysmemPartitionInfo info;
    unsigned total, largest, user_before, address;
    int rc;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    rc = record("query reserve partition 9", sceKernelQueryMemoryPartitionInfo(9, &info));
    if (rc < 0) return rc;
    total = (unsigned)sceKernelPartitionTotalFreeMemSize(9);
    largest = (unsigned)sceKernelPartitionMaxFreeMemSize(9);
    /* Keep at least 4 MiB free by this snapshot. This is a test guard, not
     * a proven budget for all stages, other games, or concurrent allocations. */
    if (!ra_extra_block_valid(info.startaddr, info.memsize, info.startaddr, RA_EXTRA_RESERVE_BYTES) ||
        total > info.memsize || largest > total || largest < RA_EXTRA_RESERVE_BYTES ||
        total < RA_EXTRA_RESERVE_BYTES + 4u * 1024u * 1024u) {
        ra_probe_log_printf(&logger, "RESERVE_SKIPPED partition9 not eligible; free=%u largest=%u\n", total, largest);
        return 1;
    }
    if (stopping || logger.error) return -34001;
    user_before = (unsigned)sceKernelPartitionTotalFreeMemSize(2);
    ra_probe_log_printf(&logger, "reserve_begin partition=9 bytes=%u free_before=%u largest_before=%u user_free_before=%u\n",
        (unsigned)RA_EXTRA_RESERVE_BYTES, total, largest, user_before);
    if (logger.error) return -1;
    /* Exactly partition 9. Never fall back to the game's partition 2. */
    reserve_id = record("allocate partition9 test block", sceKernelAllocPartitionMemory(9, "RAExtraReserve", PSP_SMEM_High, RA_EXTRA_RESERVE_BYTES, NULL));
    if (reserve_id < 0) return reserve_id;
    reserve_pointer = sceKernelGetBlockHeadAddr(reserve_id);
    address = (unsigned)(uintptr_t)reserve_pointer;
    if (!ra_extra_block_valid(info.startaddr, info.memsize, address, RA_EXTRA_RESERVE_BYTES)) {
        ra_probe_log_printf(&logger, "RESERVE_ADDRESS_REJECTED address=0x%08X; no block access\n", address);
        reserve_pointer = NULL;
        return -34102;
    }
    rc = check_owned_block(1);
    if (rc < 0) return rc;
    ra_probe_log_printf(&logger, "RESERVE_KERNEL_OK address=0x%08X bytes=%u touched_bytes=384 extra_free_after=%u user_free_after=%u user_access=UNPROVEN\n",
        address, (unsigned)RA_EXTRA_RESERVE_BYTES, (unsigned)sceKernelPartitionTotalFreeMemSize(9),
        (unsigned)sceKernelPartitionTotalFreeMemSize(2));
    return logger.error ? -1 : 0;
}
static int release_extra(void)
{
    int rc, check_result = 0;
    if (reserve_id < 0) return 0;
    if (reserve_pointer) {
        check_result = check_owned_block(0);
        record("final owned block check", check_result);
    }
    /* Observer owns this allocation; module_stop only retries after join. */
    reserve_pointer = NULL;
    rc = record("free partition9 test block", sceKernelFreePartitionMemory(reserve_id));
    if (rc >= 0) {
        reserve_id = -1;
        ra_probe_log_printf(&logger, "RESERVE_RELEASED extra_free_after=%u user_free_after=%u\n",
            (unsigned)sceKernelPartitionTotalFreeMemSize(9), (unsigned)sceKernelPartitionTotalFreeMemSize(2));
    }
    return rc < 0 ? rc : check_result;
}
typedef struct placement_memory {
    unsigned user_total, user_largest, extra_total, extra_largest;
} placement_memory;
static int placement_snapshot(const char* phase, placement_memory* memory)
{
    PspSysmemPartitionInfo partition;
    int rc;
    memset(&partition, 0, sizeof(partition)); partition.size = sizeof(partition);
    rc = sceKernelQueryMemoryPartitionInfo(2, &partition);
    if (rc < 0) return record("placement memory query partition2", rc);
    memory->user_total = (unsigned)sceKernelPartitionTotalFreeMemSize(2);
    memory->user_largest = (unsigned)sceKernelPartitionMaxFreeMemSize(2);
    if (!ra_placement2_partition_valid(partition.startaddr, partition.memsize, partition.attr) ||
        memory->user_total > partition.memsize || memory->user_largest > memory->user_total)
        return record("invalid placement user memory snapshot", -34105);
    memset(&partition, 0, sizeof(partition)); partition.size = sizeof(partition);
    rc = sceKernelQueryMemoryPartitionInfo(9, &partition);
    if (rc < 0) return record("placement memory query partition9", rc);
    memory->extra_total = (unsigned)sceKernelPartitionTotalFreeMemSize(9);
    memory->extra_largest = (unsigned)sceKernelPartitionMaxFreeMemSize(9);
    if (!ra_extra_block_valid(partition.startaddr, partition.memsize, partition.startaddr, partition.memsize) ||
        memory->extra_total > partition.memsize || memory->extra_largest > memory->extra_total)
        return record("invalid placement extra memory snapshot", -34105);
    ra_probe_log_printf(&logger, "placement_memory phase=%s user_free=%u user_largest=%u extra_free=%u extra_largest=%u\n",
        phase, memory->user_total, memory->user_largest, memory->extra_total, memory->extra_largest);
    return logger.error ? -1 : 0;
}
static void placement_delta(const char* operation, const placement_memory* before, const placement_memory* after)
{
    /* These are non-atomic snapshots; positive delta means fewer free bytes.
     * The game may also allocate/free between calls, so this is not attribution. */
    ra_probe_log_printf(&logger, "placement_memory_delta operation=%s user_consumed=%d extra_consumed=%d attribution=UNPROVEN\n",
        operation, (int)before->user_total - (int)after->user_total,
        (int)before->extra_total - (int)after->extra_total);
}
static int load_placement(const char** stage)
{
    SceKernelLMOption option;
    SceKernelModuleInfo module;
    PspSysmemPartitionInfo partition;
    placement_memory before, after;
    unsigned i, valid_segments = 0, segment_bytes = 0;
    int rc;
    *stage = "placement_query_partition";
    memset(&partition, 0, sizeof(partition)); partition.size = sizeof(partition);
    rc = record("query placement partition2", sceKernelQueryMemoryPartitionInfo(2, &partition));
    if (rc < 0) return rc;
    *stage = "placement_preflight";
    if (stopping || logger.error) return -34001;
    if (!ra_placement2_partition_valid(partition.startaddr, partition.memsize, partition.attr)) {
        ra_probe_log_printf(&logger, "PLACEMENT_UNSUPPORTED partition=2 attr=0x%08X start=0x%08X size=%u reason=%s load=NOT_ATTEMPTED execution=UNTESTED stack=UNTESTED\n",
            (unsigned)partition.attr, (unsigned)partition.startaddr, (unsigned)partition.memsize,
            (partition.attr & 3u) ? "UNSUPPORTED_LAYOUT" : "USER_PARTITION_MISMATCH");
        return logger.error ? -1 : RA_EXTRA_PLACEMENT_UNSUPPORTED;
    }
    *stage = "placement_memory_before_load";
    rc = placement_snapshot("before_load", &before);
    if (rc < 0) return rc;
    if (!ra_placement2_can_load(partition.memsize, before.user_total, before.user_largest)) {
        ra_probe_log_printf(&logger, "USER_MEMORY_GUARD phase=before_load free=%u largest=%u module_budget=%u floor=%u load=NOT_ATTEMPTED\n",
            before.user_total, before.user_largest, (unsigned)RA_PLACEMENT2_MODULE_BUDGET, (unsigned)RA_PLACEMENT2_FREE_FLOOR);
        return logger.error ? -1 : RA_PLACEMENT2_MEMORY_GUARD;
    }
    memset(&option, 0, sizeof(option)); option.size = sizeof(option);
    option.mpidtext = 2; option.mpiddata = 2; option.position = 1; option.access = 1;
    ra_probe_log_printf(&logger, "placement_begin text_partition=2 data_partition=2 start=NEVER user_free_before=%u\n", before.user_total);
    if (stopping || logger.error) return -34001;
    *stage = "load_placement";
    placement_module = record("load placement user PRX (NO START)", sceKernelLoadModule(
        "ms0:/seplugins/RAInGameLite/RAPlacementUser.prx", 0, &option));
    *stage = "placement_memory_after_load";
    rc = placement_snapshot("after_load", &after);
    if (rc >= 0) placement_delta("load", &before, &after);
    if (placement_module < 0) { *stage = "load_placement"; return placement_module; }
    if (rc < 0) return rc;
    if (!ra_placement2_can_hold(partition.memsize, after.user_total, after.user_largest) ||
        (before.user_total > after.user_total && before.user_total - after.user_total > RA_PLACEMENT2_MODULE_BUDGET)) {
        ra_probe_log_printf(&logger, "USER_MEMORY_GUARD phase=after_load module_budget=%u attribution=UNPROVEN; unload child\n",
            (unsigned)RA_PLACEMENT2_MODULE_BUDGET);
        return logger.error ? -1 : RA_PLACEMENT2_MEMORY_GUARD;
    }
    *stage = "placement_query_module";
    memset(&module, 0, sizeof(module)); module.size = sizeof(module);
    rc = record("query loaded placement", sceKernelQueryModuleInfo(placement_module, &module));
    if (rc < 0) return rc;
    ra_probe_log_printf(&logger, "loaded_name=%.27s attr=0x%08X text=0x%08X text_size=%u data_size=%u bss_size=%u segments=%u entry=0x%08X gp=0x%08X\n",
        module.name, (unsigned)module.attribute, (unsigned)module.text_addr, (unsigned)module.text_size,
        (unsigned)module.data_size, (unsigned)module.bss_size, (unsigned)module.nsegment,
        (unsigned)module.entry_addr, (unsigned)module.gp_value);
    *stage = "placement_validate";
    if (strncmp(module.name, "RAPlacementUser", sizeof("RAPlacementUser")) ||
        module.attribute != 0 || module.nsegment < 1 || module.nsegment > 4 ||
        !ra_placement2_module_sizes_valid(module.text_size, module.data_size, module.bss_size) ||
        !ra_placement2_segment_valid(partition.startaddr, partition.memsize, module.text_addr, module.text_size))
        return -34104;
    for (i = 0; i < (unsigned)module.nsegment; ++i) {
        unsigned address = (unsigned)module.segmentaddr[i], size = (unsigned)module.segmentsize[i];
        ra_probe_log_printf(&logger, "placement_segment=%u address=0x%08X size=%u\n", i, address, size);
        if (!size) continue;
        if (!ra_placement2_segment_valid(partition.startaddr, partition.memsize, address, size) ||
            size > RA_PLACEMENT2_MODULE_BUDGET - segment_bytes) return -34104;
        segment_bytes += size;
        ++valid_segments;
    }
    if (!valid_segments) return -34104;
    ra_probe_log_printf(&logger, "PLACEMENT_USER2_LOAD_OK segments=%u segment_bytes=%u user_free_after=%u extra_free_after=%u execution=UNTESTED stack=UNTESTED extra_user_access=UNTESTED\n",
        valid_segments, segment_bytes, after.user_total, after.extra_total);
    return logger.error ? -1 : 0;
}
static int unload_placement(void)
{
    placement_memory before, after;
    int rc, before_rc, after_rc;
    if (placement_module < 0) return 0;
    /* This child was never started. No StopModule call or user callback. */
    before_rc = placement_snapshot("before_unload", &before);
    /* Measurement/log errors must never prevent releasing the unstarted child. */
    rc = record("unload unstarted placement user PRX", sceKernelUnloadModule(placement_module));
    if (rc >= 0) placement_module = -1;
    after_rc = placement_snapshot("after_unload", &after);
    if (before_rc >= 0 && after_rc >= 0) placement_delta("unload", &before, &after);
    return rc < 0 ? rc : before_rc < 0 ? before_rc : after_rc < 0 ? after_rc : rc;
}
static int observer(SceSize args, void* argp)
{
    unsigned tries, second, minimum_total = 0, minimum_max = 0;
    int result = 0;
    const char* stage = "target_wait";
    (void)args; (void)argp;
    log_fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0) return log_fd;
    ra_probe_log_init(&logger, log_write, NULL);
    ra_probe_log_printf(&logger, "probe_version=0.8-placement-user2 module=RAInGameLite target=LocoRoco\n");
    ra_probe_log_printf(&logger, "mode=partition2-load-only reserve_partition=9 worker=ABSENT network=ABSENT direct_game_reads=ABSENT callbacks=ABSENT\n");
    ra_probe_log_printf(&logger, "thread_priority=0x60 stack_bytes=8192 period_ms=1000 duration_s=120\n");
    for (tries = 0; tries < 120 && !stopping; ++tries) {
        if (sceKernelFindModuleByName("LocoRoco") != NULL) break;
        sceKernelDelayThread(250000);
    }
    if (tries == 120 || stopping) { result = -34003; record("target unavailable or stopped", result); goto done; }
    ra_probe_log_printf(&logger, "target_module_observed; begin memory-only baseline\n");
    for (second = 0; second <= 120 && !stopping; ++second) {
        stage = "sample_memory";
        result = sample_memory(second, &minimum_total, &minimum_max);
        if (result != 0 || second == 120) break;
        if (second == 10) {
            stage = "reserve_extra";
            result = reserve_extra(); if (result != 0) break;
            result = load_placement(&stage); if (result != 0) break;
        }
        else if (reserve_id >= 0 && second % 10 == 0) {
            stage = "check_owned_block";
            result = check_owned_block(0);
            ra_probe_log_printf(&logger, "owned_block_check second=%u result=%d\n", second, result);
            if (result < 0) break;
        }
        if (second == 0 || second == 3 || second == 10 || second == 30 || second == 60)
            sample_layout(second);
        stage = "observe";
        if (logger.error || stopping) { result = logger.error ? -1 : 0; break; }
        /* No busy poll, callbacks or VBlank wait. Sample labels describe
         * scheduled intervals, not exact elapsed time or game frames. */
        sceKernelDelayThread(1000000);
    }
done:
    {
        int unload_result = unload_placement();
        int release_result = release_extra();
        ra_probe_log_printf(&logger, "cleanup probe_result=0x%08X probe_stage=%s unload_result=0x%08X release_result=0x%08X\n",
            (unsigned)result, stage, (unsigned)unload_result, (unsigned)release_result);
        if (unload_result < 0) { result = unload_result; stage = "unload_placement"; }
        if (release_result < 0) { result = release_result; stage = "release_extra"; }
    }
    ra_probe_log_printf(&logger, "lite_finished stop=%u result=%d result_hex=0x%08X error=%s stage=%s outcome=%s log_error=0x%08X frame_binding=UNPROVEN\n",
        (unsigned)stopping, result, (unsigned)result, result_name(result), stage,
        result == RA_EXTRA_PLACEMENT_UNSUPPORTED ? "UNSUPPORTED" : result == RA_PLACEMENT2_MEMORY_GUARD ? "MEMORY_GUARD" :
        result ? "FAILED" : stopping ? "STOPPED" : "COMPLETE",
        (unsigned)logger.error);
    sceIoClose(log_fd); log_fd = -1;
    return result;
}
int module_start(SceSize args, void* argp)
{
    int rc;
    (void)args; (void)argp;
    observer_thread = sceKernelCreateThread("RAInGameLite", observer, 0x60, 8192, 0, NULL);
    if (observer_thread < 0) return observer_thread;
    rc = sceKernelStartThread(observer_thread, 0, NULL);
    if (rc < 0) { sceKernelDeleteThread(observer_thread); observer_thread = -1; }
    return rc;
}
int module_stop(SceSize args, void* argp)
{
    SceUInt timeout = 3000000;
    int rc;
    (void)args; (void)argp;
    stopping = 1;
    if (observer_thread < 0) return 0;
    rc = sceKernelWaitThreadEnd(observer_thread, &timeout);
    if (rc < 0) return rc; /* Refuse to unload a thread still writing a log. */
    if (placement_module >= 0) {
        rc = sceKernelUnloadModule(placement_module);
        if (rc < 0) return rc;
        placement_module = -1;
    }
    if (reserve_id >= 0) {
        rc = sceKernelFreePartitionMemory(reserve_id);
        if (rc < 0) return rc;
        reserve_id = -1;
    }
    rc = sceKernelDeleteThread(observer_thread);
    if (rc >= 0) observer_thread = -1;
    return rc;
}
