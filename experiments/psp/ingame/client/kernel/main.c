#ifdef RA_CLIENT_HOST_TEST
#include "lite_test_sdk.h"
#else
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <pspkerror.h>
#endif
#include <string.h>
#include "probe_log.h"
#include "client_shared.h"
#include "placement2_policy.h"
static ra_client_control* control;
#ifdef RA_CLIENT_HTTPS
static SceUID game_uid = -1;
#endif
PSP_MODULE_INFO("RAClientBootstrap", PSP_MODULE_KERNEL, 0, RA_CLIENT_MINOR);
#ifdef RA_CLIENT_HTTPS
int sceUtilsBufferCopyWithRange(void*, int, void*, int, int);
static void erase_seed(void* data, unsigned size)
{
    volatile unsigned char* bytes = data;
    while (size--) *bytes++ = 0;
}
static int prepare_https(void)
{
    unsigned char bytes[32] __attribute__((aligned(64)));
    SceUID ids[256];
    unsigned offset, i;
    int count = 0, rc;
    control->entropy_status = -1;
    rc = sceKernelGetModuleIdList(ids, sizeof(ids), &count);
    if (rc < 0 || count < 0 || count > 256) return -35301;
    for (i = 0; i < (unsigned)count; ++i) {
        SceModule* module = sceKernelFindModuleByUID(ids[i]);
        if (module && (!strncmp(module->modname, "sceNet", 6) || !strncmp(module->modname, "sceHttp", 7)))
            return -35301; /* Do not initialize or terminate another owner's network. */
    }
    for (offset = 0; offset < sizeof(control->entropy); offset += 20) {
        unsigned nonzero = 0;
        memset(bytes, 0, sizeof(bytes));
        rc = sceUtilsBufferCopyWithRange(bytes, 20, NULL, 0, 14);
        for (i = 0; i < 20; ++i) nonzero |= bytes[i];
        if (rc || !nonzero || (offset && !memcmp(bytes, control->entropy + offset - 20, 20))) {
            erase_seed(bytes, sizeof(bytes)); erase_seed(control->entropy, sizeof(control->entropy));
            return -35302;
        }
        memcpy(control->entropy + offset, bytes, 20);
    }
    erase_seed(bytes, sizeof(bytes)); control->entropy_status = 0;
    return 0;
}
#endif
static volatile int stopping;
static SceUID thread_id = -1, child = -1, block = -1, log_fd = -1;
static int start_attempted;
static int observer_result;
#ifdef RA_CLIENT_HTTPS
#define RA_INPUT_METHOD_SLOT UINT32_C(0x089B4EE4)
#define RA_INPUT_METHOD_ORIGINAL UINT32_C(0x08842D90)
static int probe_hook_installed;
static int probe_was_installed;
#endif
static ra_probe_log logger;
static unsigned written;
static int log_write(const void* data, unsigned size, void* unused)
{
    int rc;
    (void)unused;
    if (size > RA_CLIENT_BOOT_LOG_LIMIT - written) return (int)size;
    rc = sceIoWrite(log_fd, data, size);
    if (rc > 0) written += (unsigned)rc;
    return rc;
}
static int record(const char* name, int rc)
{ ra_probe_log_printf(&logger, "%s: 0x%08X\n", name, (unsigned)rc); return rc; }
#ifdef RA_CLIENT_HTTPS
#include "../../https/kernel/game_audit.h"
static int publish_game_map(void)
{
    SceModule* game = sceKernelFindModuleByName("LocoRoco");
    SceKernelModuleInfo info;
    ra_game_map map;
    unsigned i;
    int rc;
    if (!game || game->modid == child) return -35409;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    rc = record("query game memory map", sceKernelQueryModuleInfo(game->modid, &info));
    if (rc < 0) return rc;
    if (strcmp(info.name, "LocoRoco") || (info.attribute & PSP_MODULE_KERNEL) ||
        info.nsegment < 1 || (unsigned)info.nsegment > RA_GAME_SEGMENTS)
        return -35409;
    memset(&map, 0, sizeof(map)); map.count = info.nsegment;
    for (i = 0; i < map.count; ++i) {
        map.regions[i].address = (uint32_t)info.segmentaddr[i];
        map.regions[i].size = (uint32_t)info.segmentsize[i];
        ra_probe_log_printf(&logger, "GAME_SEGMENT index=%u address=0x%08X size=%u\n",
            i, map.regions[i].address, map.regions[i].size);
    }
    if (!ra_game_map_valid(&map) || !ra_game_map_contains(&map, info.text_addr, info.text_size)) return -35409;
    control->game_map = map;
    game_uid = game->modid;
    audit_block(game,RA_CLIENT_REQUEST_SECOND);
    audit_game_code(game,&info);
    __sync_synchronize();
    return logger.error;
}
static int game_map_live(void)
{
    SceModule* game = sceKernelFindModuleByName("LocoRoco");
    SceKernelModuleInfo info;
    unsigned i;
    int rc;
    if (!game || game->modid != game_uid) return -35409;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    rc = sceKernelQueryModuleInfo(game_uid, &info); if (rc < 0) return rc;
    if (strcmp(info.name, "LocoRoco") || (info.attribute & PSP_MODULE_KERNEL) ||
        (unsigned)info.nsegment != control->game_map.count) return -35409;
    for (i = 0; i < control->game_map.count; ++i)
        if ((uint32_t)info.segmentaddr[i] != control->game_map.regions[i].address ||
            (uint32_t)info.segmentsize[i] != control->game_map.regions[i].size) return -35409;
    return 0;
}
#include "../../https/kernel/notice_hook.h"
#if RA_CLIENT_HOOK_ENABLED
static int install_runtime_probe(void)
{
    SceKernelModuleInfo client_info;
    volatile uint32_t* slot=(volatile uint32_t*)(uintptr_t)RA_INPUT_METHOD_SLOT;
    uint32_t wrapper;
    int intr,rc;
    if(!control || child<0 || !ra_game_map_contains(&control->game_map,RA_INPUT_METHOD_SLOT,4) ||
       !ra_game_map_contains(&control->game_map,UINT32_C(0x08842CA0),4) ||
       *(const volatile uint32_t*)(uintptr_t)UINT32_C(0x08842CA0)!=UINT32_C(0x0E2105E2))return -35409;
    memset(&client_info,0,sizeof(client_info));client_info.size=sizeof(client_info);
    rc=sceKernelQueryModuleInfo(child,&client_info);if(rc<0)return rc;
    wrapper=control->probe_wrapper;
    if(wrapper%4 || wrapper<client_info.text_addr || client_info.text_size<4 ||
       wrapper-client_info.text_addr>client_info.text_size-4)return -35409;
    intr=sceKernelCpuSuspendIntr();
    if(*slot!=RA_INPUT_METHOD_ORIGINAL){sceKernelCpuResumeIntr(intr);return -35409;}
    *slot=wrapper;
    sceKernelDcacheWritebackInvalidateRange((void*)(uintptr_t)RA_INPUT_METHOD_SLOT,4);
    __sync_synchronize();probe_hook_installed=1;probe_was_installed=1;control->probe_installed=1;
    sceKernelCpuResumeIntr(intr);
    ra_probe_log_printf(&logger,"RUNTIME_PROBE_INSTALL slot=0x%08X original=0x%08X wrapper=0x%08X result=0\n",
        RA_INPUT_METHOD_SLOT,RA_INPUT_METHOD_ORIGINAL,wrapper);
    return logger.error;
}
static int remove_runtime_probe(void)
{
    volatile uint32_t* slot=(volatile uint32_t*)(uintptr_t)RA_INPUT_METHOD_SLOT;
    unsigned waits;uint32_t wrapper;int intr;
    if(!probe_hook_installed)return 0;
    if(!control)return -35409;
    wrapper=control->probe_wrapper;
    intr=sceKernelCpuSuspendIntr();
    if(*slot!=wrapper){sceKernelCpuResumeIntr(intr);return -35409;}
    *slot=RA_INPUT_METHOD_ORIGINAL;
    sceKernelDcacheWritebackInvalidateRange((void*)(uintptr_t)RA_INPUT_METHOD_SLOT,4);
    __sync_synchronize();probe_hook_installed=0;control->probe_installed=0;
    sceKernelCpuResumeIntr(intr);
    sceKernelDelayThread(100000);
    for(waits=0;waits<100 && control->probe_active;++waits)sceKernelDelayThread(10000);
    ra_probe_log_printf(&logger,"RUNTIME_PROBE_RESTORE slot=0x%08X value=0x%08X active=%u wait_ms=%u result=%d\n",
        RA_INPUT_METHOD_SLOT,(unsigned)*slot,(unsigned)control->probe_active,100+waits*10,
        control->probe_active?-35409:0);
    return control->probe_active || logger.error ? -35409 : 0;
}
#else
static int remove_runtime_probe(void){return 0;}
#endif
#endif
static void request_stop(void)
{
    int intr = sceKernelCpuSuspendIntr();
    if (control) control->stop = 1;
    sceKernelCpuResumeIntr(intr);
}
static int snapshot(const char* phase, unsigned second, unsigned* free_out)
{
    PspSysmemPartitionInfo partition;
    unsigned total, largest;
    int rc;
    memset(&partition, 0, sizeof(partition)); partition.size = sizeof(partition);
    rc = sceKernelQueryMemoryPartitionInfo(2, &partition);
    if (rc < 0) return rc;
    total = (unsigned)sceKernelPartitionTotalFreeMemSize(2);
    largest = (unsigned)sceKernelPartitionMaxFreeMemSize(2);
    if (!ra_client_partition_valid(partition.startaddr, partition.memsize, partition.attr)) {
        ra_probe_log_printf(&logger, "USER_LAYOUT_UNSUPPORTED start=0x%08X size=%u attr=0x%08X required_size=%u\n",
            partition.startaddr, partition.memsize, partition.attr, RA_CLIENT_USER_SIZE);
        return -35102;
    }
    if (total > partition.memsize || largest > total) return -35102;
    if (phase) ra_probe_log_printf(&logger, "memory phase=%s second=%u user_free=%u user_largest=%u kernel_free=%u ready=%u done=%u idle=%u frames=%u user_size=%u\n",
        phase, second, total, largest, (unsigned)sceKernelPartitionTotalFreeMemSize(1),
        control ? (unsigned)control->ready : 0, control ? (unsigned)control->done : 0,
        control ? (unsigned)control->idle_calls : 0, control ? (unsigned)control->frame_calls : 0, partition.memsize);
    if (free_out) *free_out = total;
    if (logger.error) return logger.error;
    if (total < RA_CLIENT_FREE_FLOOR || largest < RA_CLIENT_ENTRY_STACK) return -35103;
    return 0;
}
static int validate_child(void)
{
    SceKernelModuleInfo info;
    unsigned i, bytes = 0, text_covered = 0, entry;
    int rc;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    rc = record("query client module", sceKernelQueryModuleInfo(child, &info));
    if (rc < 0) return rc;
    ra_probe_log_printf(&logger, "loaded_name=%.27s attr=0x%08X text=%u data=%u bss=%u segments=%u\n",
        info.name, (unsigned)info.attribute, (unsigned)info.text_size, (unsigned)info.data_size,
        (unsigned)info.bss_size, (unsigned)info.nsegment);
    entry = (unsigned)info.entry_addr;
    if (strcmp(info.name, "RAClientUser") || info.attribute || info.nsegment < 1 || info.nsegment > 4 ||
        info.text_size < 4 || (uint64_t)info.text_size + info.data_size + info.bss_size > RA_CLIENT_MODULE_LIMIT ||
        !ra_client_user_range(info.text_addr, info.text_size) ||
        entry % 4 || entry < info.text_addr || entry - info.text_addr > info.text_size - 4)
        return -35104;
    for (i = 0; i < (unsigned)info.nsegment; ++i) {
        unsigned address = (unsigned)info.segmentaddr[i], size = (unsigned)info.segmentsize[i];
        ra_probe_log_printf(&logger, "client_segment=%u address=0x%08X size=%u\n", i, address, size);
        if (!size) continue;
        if (!ra_client_user_range(address, size) ||
            size > RA_CLIENT_MODULE_LIMIT - bytes) return -35104;
        bytes += size;
        if (info.text_addr >= address && info.text_addr - address < size &&
            info.text_size <= size - (info.text_addr - address)) text_covered = 1;
    }
    return bytes && text_covered && !logger.error ? 0 : -35104;
}
static void stack_option(SceKernelSMOption* option)
{
    memset(option, 0, sizeof(*option)); option->size = sizeof(*option);
    option->mpidstack = 2; option->stacksize = RA_CLIENT_ENTRY_STACK;
    /* ModuleMgr derives the privilege level from the module. USER is a
     * CreateThread attribute, forbidden in SceKernelSMOption. */
    option->priority = 0x60; option->attribute = 0;
}
#ifdef RA_CLIENT_HTTPS
static void stop_diagnostics(void)
{
    static const char* const stages[] = { "NONE", "ENTER", "WAIT", "WAIT_ERROR", "NOT_DONE",
        "DELETE", "DELETE_ERROR", "FINISHED", "NO_THREAD", "REFER_ERROR", "THREAD_RUNNING" };
    int stage;
    if (!control) return;
    stage = control->stop_stage;
    ra_probe_log_printf(&logger, "client_stop_diag stage=%s(%d) owner=0x%08X caller=0x%08X wait=0x%08X delete=0x%08X timeout_us=%u done=%u worker_result=0x%08X free_before_deinit=%u free_after_deinit=%u\n",
        (unsigned)stage < sizeof(stages)/sizeof(stages[0]) ? stages[stage] : "INVALID", stage,
        (unsigned)control->owner_thread_id, (unsigned)control->stop_caller_id,
        (unsigned)control->stop_wait_result, (unsigned)control->stop_delete_result,
        (unsigned)control->stop_timeout_us, (unsigned)control->done, (unsigned)control->result,
        (unsigned)control->free_before_deinit, (unsigned)control->free_after_deinit);
    ra_probe_log_printf(&logger, "client_stop_thread refer=0x%08X status=0x%08X exit=0x%08X\n",
        (unsigned)control->stop_refer_result, (unsigned)control->stop_thread_status, (unsigned)control->stop_exit_status);
}
#endif
static int cleanup(void)
{
    SceKernelSMOption option;
    int rc, status = -1;
#ifdef RA_CLIENT_HTTPS
    /* A menu callback may be cooperatively parked outside engine_gate.
     * Signal it before waiting for probe_active during slot restoration. */
    if(control)control->stop=1;
    rc=remove_notice_hook();if(rc<0)return rc;
    rc=remove_runtime_probe();if(rc<0)return rc;
    /* A thread could have fetched the wrapper before slot restoration and
     * still be suspended before entering it. No finite grace period proves
     * quiescence. Keep user code AND its control until game-process teardown. */
    if(probe_was_installed && child>=0){
        ra_probe_log_printf(&logger,"RUNTIME_PROBE_RETAIN user=RESIDENT control=RESIDENT reason=prefetched_wrapper lifetime=GAME_PROCESS\n");
        return 0;
    }
#endif
    request_stop();
    if (child >= 0 && start_attempted) {
        stack_option(&option);
        rc = record("stop client module", sceKernelStopModule(child, 0, NULL, &status, &option));
        record("client stop status", status);
#ifdef RA_CLIENT_HTTPS
        stop_diagnostics();
#endif
        /* A rejected StartModule may leave a relocated, unstarted module.
         * Only ModuleMgr's explicit NOT_STARTED allows skipping module_stop;
         * UnloadModule must still succeed before control can be freed. */
        if ((unsigned)rc == (unsigned)SCE_KERNEL_ERROR_NOT_STARTED)
            ra_probe_log_printf(&logger, "client not started; unload permitted by ModuleMgr\n");
        else if (rc < 0 || status != 0)
            return rc < 0 ? rc : -35105;
        start_attempted = 0;
    }
    if (child >= 0) {
        rc = record("unload client module", sceKernelUnloadModule(child));
        if (rc < 0) return rc;
        child = -1;
    }
    if (block >= 0) {
#ifdef RA_CLIENT_HTTPS
        if (control) erase_seed(control->entropy, sizeof(control->entropy));
#endif
        int intr = sceKernelCpuSuspendIntr();
        control = NULL;
        sceKernelCpuResumeIntr(intr);
        rc = record("free client control", sceKernelFreePartitionMemory(block));
        if (rc < 0) return rc;
        block = -1;
    }
    return 0;
}
static int start_client(unsigned second)
{
    SceKernelLMOption load;
    SceKernelSMOption start;
    ra_client_args args;
    SceIoStat file_info;
    uintptr_t address;
    unsigned total = 0;
    int rc, status = -1;
    rc = snapshot("before_load", second, &total);
    if (rc < 0) return rc;
#ifdef RA_CLIENT_HTTPS
    if (total < RA_CLIENT_EARLY_FREE) {
        ra_probe_log_printf(&logger, "EARLY_WINDOW_MISSED free=%u required=%u load=NOT_ATTEMPTED\n",
            total, RA_CLIENT_EARLY_FREE);
        return -35316;
    }
#endif
    if (total < RA_CLIENT_START_FREE || (unsigned)sceKernelPartitionMaxFreeMemSize(2) < RA_CLIENT_MODULE_LIMIT)
        return record("insufficient startup memory", -35103);
    /* Account conservatively for the PRX input buffer plus relocated image.
     * This is not a quota on the loader's other temporary allocations. */
    memset(&file_info, 0, sizeof(file_info));
    rc = record("query client PRX file", sceIoGetstat(RA_CLIENT_DIR "RAClientUser.prx", &file_info));
    if (rc < 0) return rc;
    if (file_info.st_size <= 0 || file_info.st_size > RA_CLIENT_FILE_LIMIT) return -35104;
    ra_probe_log_printf(&logger, "client_file_bytes=%u file_limit=%u startup_free_guard=%u\n",
        (unsigned)file_info.st_size, RA_CLIENT_FILE_LIMIT, RA_CLIENT_START_FREE);
    block = record("allocate client control", sceKernelAllocPartitionMemory(2, "RAClientControl", PSP_SMEM_Low, sizeof(*control), NULL));
    if (block < 0) return block;
    control = sceKernelGetBlockHeadAddr(block); address = (uintptr_t)control;
    if (!ra_client_user_range((uint32_t)address, sizeof(*control))) {
        control = NULL; return -35102;
    }
    memset(control, 0, sizeof(*control)); control->magic = RA_CLIENT_MAGIC; control->size = sizeof(*control);
#ifdef RA_CLIENT_HTTPS
    control->owner_thread_id = control->stop_caller_id = -1;
    control->stop_wait_result = control->stop_delete_result = RA_STOP_PENDING;
    control->stop_refer_result = RA_STOP_PENDING;
    rc = record("prepare HTTPS entropy/network gate", prepare_https()); if (rc < 0) return rc;
#endif
    memset(&load, 0, sizeof(load)); load.size = sizeof(load);
    load.mpidtext = load.mpiddata = 2; load.position = 1; load.access = 1;
    if (stopping || logger.error) return -35106;
    child = record("load client user", sceKernelLoadModule(RA_CLIENT_DIR "RAClientUser.prx", 0, &load));
    if (child < 0) return child;
    rc = validate_child(); if (rc < 0) return rc;
    rc = snapshot("after_load", second, &total); if (rc < 0) return rc;
    if (total < RA_CLIENT_FREE_FLOOR + RA_CLIENT_HEAP_BYTES + RA_CLIENT_STACK_BYTES + RA_CLIENT_ENTRY_STACK + 4096u)
        return record("insufficient worker startup memory", -35103);
    if (stopping || logger.error) return -35106;
    args.control = control; stack_option(&start);
    ra_probe_log_printf(&logger, "client_sm_option stack_partition=%d stack_bytes=%u priority=%d attribute=0x%08X\n",
        start.mpidstack, (unsigned)start.stacksize, start.priority, start.attribute);
    if (logger.error) return logger.error;
    start_attempted = 1;
    rc = record("start client user", sceKernelStartModule(child, sizeof(args), &args, &status, &start));
    record("client start status", status);
    return rc < 0 ? rc : status;
}
static int observer(SceSize args, void* argp)
{
    unsigned tries, second = 0;
    int result = 0, cleanup_result, memory_result;
    (void)args; (void)argp;
    log_fd = sceIoOpen(RA_CLIENT_DIR "client-bootstrap.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (log_fd < 0) { observer_result = log_fd; return 0; }
    ra_probe_log_init(&logger, log_write, NULL);
    ra_probe_log_printf(&logger, "client_version=%s target=LocoRoco runtime=%s mode=USER_LOG hardcore=0\n", RA_CLIENT_VERSION, RA_CLIENT_RUNTIME);
    ra_probe_log_printf(&logger, "module_limit=%u heap=%u owner_stack=%u entry_stack=%u free_floor=%u frames=CANDIDATE\n",
        RA_CLIENT_MODULE_LIMIT, RA_CLIENT_HEAP_BYTES, RA_CLIENT_STACK_BYTES, RA_CLIENT_ENTRY_STACK, RA_CLIENT_FREE_FLOOR);
    ra_probe_log_printf(&logger, "required_user_partition_bytes=%u (ARK +16MiB for HTTPS build)\n", RA_CLIENT_USER_SIZE);
#ifdef RA_CLIENT_HTTPS
    ra_probe_log_printf(&logger, "startup=EARLY load_second=%u request_second=%u early_free_guard=%u\n",
        RA_CLIENT_LOAD_SECOND, RA_CLIENT_REQUEST_SECOND, RA_CLIENT_EARLY_FREE);
#endif
    for (tries = 0; tries < 120 && !stopping; ++tries) {
        if (sceKernelFindModuleByName("LocoRoco")) break;
        sceKernelDelayThread(250000);
    }
    if (tries == 120 || stopping) { result = -35106; goto done; }
#ifdef RA_CLIENT_HTTPS
    for (second = 0; !stopping; ++second) {
#else
    for (second = 0; second <= RA_CLIENT_OBSERVE_SECONDS && !stopping; ++second) {
#endif
        result = snapshot(second <= 20 || second % 60 == 0 ? "observe" : NULL, second, NULL);
        if (result < 0) break;
        if (second == RA_CLIENT_LOAD_SECOND) { result = start_client(second); if (result != 0) break; }
#ifdef RA_CLIENT_HTTPS
        if (second >= RA_CLIENT_REQUEST_SECOND && control && !control->request_allowed) {
            result = publish_game_map(); if (result) break;
#if RA_CLIENT_HOOK_ENABLED
            result = install_runtime_probe(); if (result) break;
            {
                int notice_result=install_notice_hook();
                if(notice_result)record("NOTICE_HOOK_UNAVAILABLE",notice_result);
            }
#endif
            control->request_allowed = 1;
            ra_probe_log_printf(&logger, "HTTPS_REQUEST_RELEASE second=%u network_ready=%u\n", second, (unsigned)control->ready);
        }
        if (control && control->request_allowed && !control->done) {
            result = game_map_live();
            if (result) { record("game memory map changed/unavailable", result); break; }
            if(second==60){
                SceModule* game=sceKernelFindModuleByName("LocoRoco");
                if(game && game->modid==game_uid)audit_block(game,second);
            }
        }
#endif
        if (control && control->done) { result = control->result; break; }
        if (control && control->stop) { result = control->result ? control->result : -35106; break; }
        if (second >= 20 && control && !control->ready) { result = -35107; break; }
#ifndef RA_CLIENT_HTTPS
        if (second == RA_CLIENT_OBSERVE_SECONDS) break;
#endif
        sceKernelDelayThread(1000000);
    }
done:
    cleanup_result = cleanup();
    memory_result = record("post cleanup memory", snapshot("after_cleanup", second, NULL));
    if (!result && memory_result < 0) result = memory_result;
    ra_probe_log_printf(&logger, "client_bootstrap_finished result=%d result_hex=0x%08X cleanup_result=%d log_error=0x%08X frame_binding=UNPROVEN\n",
        result, (unsigned)result, cleanup_result, (unsigned)logger.error);
    {
        int close_result = sceIoClose(log_fd);
        if (close_result >= 0) log_fd = -1;
        else if (!result) result = close_result;
    }
    /* ThreadMan exit status is separate from the experiment result. Negative
     * thread exits can become ILLEGAL_ARGUMENT and prevent the later join. */
    observer_result = cleanup_result < 0 ? cleanup_result : result;
    return 0;
}
int module_start(SceSize args, void* argp)
{
    int rc;
    (void)args; (void)argp;
    thread_id = sceKernelCreateThread("RAClientBootstrap", observer, 0x60, 8192, 0, NULL);
    if (thread_id < 0) return thread_id;
    rc = sceKernelStartThread(thread_id, 0, NULL);
    if (rc < 0) { sceKernelDeleteThread(thread_id); thread_id = -1; }
    return rc;
}
int module_stop(SceSize args, void* argp)
{
    SceUInt timeout = 5000000;
    int rc;
    (void)args; (void)argp;
    stopping = 1; request_stop();
    if (thread_id >= 0) {
        rc = sceKernelWaitThreadEnd(thread_id, &timeout);
        if (rc < 0) return rc;
    }
    /* The observer closed its log; retries must not use the stale writer. */
    logger.write = NULL;
    rc = cleanup(); if (rc < 0) return rc;
    if (log_fd >= 0) {
        rc = sceIoClose(log_fd); if (rc < 0) return rc;
        log_fd = -1;
    }
    if (thread_id < 0) return 0;
    rc = sceKernelDeleteThread(thread_id);
    if (rc >= 0) thread_id = -1;
    return rc;
}
