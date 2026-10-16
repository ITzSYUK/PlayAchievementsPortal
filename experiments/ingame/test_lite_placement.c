#define RA_LITE_HOST_TEST 1
#include "../psp/ingame/lite/main.c"
#include <assert.h>
#include <stdio.h>
static struct {
    unsigned free_before, consumed, load_calls, unload_calls;
    int loaded, load_error, query_error, unload_error, partition_error, bad_segment;
    int log_error, fail_log_after_load;
    char log[32768];
    unsigned log_length;
} mock;
static void reset(void)
{
    memset(&mock, 0, sizeof(mock));
    mock.free_before = 864512; mock.consumed = 4096;
    stopping = 0; placement_module = -1; reserve_id = -1; reserve_pointer = NULL;
    ra_probe_log_init(&logger, log_write, NULL); written = 0; log_fd = 1;
}
int sceKernelQueryMemoryPartitionInfo(int pid, PspSysmemPartitionInfo* info)
{
    if (mock.partition_error) { int error = mock.partition_error; mock.partition_error = 0; return error; }
    assert(pid == 2 || pid == 9);
    info->startaddr = pid == 2 ? 0x08800000u : 0x0A000000u;
    info->memsize = 25165824; info->attr = pid == 2 ? 0x0F : 0x0C;
    return 0;
}
unsigned sceKernelPartitionTotalFreeMemSize(int pid)
{
    assert(pid == 2 || pid == 9);
    return pid == 2 ? mock.free_before - (mock.loaded ? mock.consumed : 0) : 14677760;
}
unsigned sceKernelPartitionMaxFreeMemSize(int pid) { return sceKernelPartitionTotalFreeMemSize(pid); }
int sceKernelLoadModule(const char* path, int flags, SceKernelLMOption* option)
{
    assert(strcmp(path, "ms0:/seplugins/RAInGameLite/RAPlacementUser.prx") == 0 && flags == 0);
    assert(option->mpidtext == 2 && option->mpiddata == 2);
    ++mock.load_calls;
    if (mock.load_error) return mock.load_error;
    mock.loaded = 1;
    if (mock.fail_log_after_load) mock.log_error = -77;
    return 42;
}
int sceKernelQueryModuleInfo(int uid, SceKernelModuleInfo* info)
{
    assert(uid == 42 && mock.loaded);
    if (mock.query_error) return mock.query_error;
    strcpy(info->name, "RAPlacementUser"); info->attribute = 0;
    info->text_addr = 0x08804000; info->text_size = 172; info->data_size = info->bss_size = 1024;
    info->nsegment = 1; info->segmentaddr[0] = mock.bad_segment ? 0x0A000000u : info->text_addr;
    info->segmentsize[0] = 2220;
    return 0;
}
int sceKernelUnloadModule(int uid)
{
    assert(uid == 42 && mock.loaded); ++mock.unload_calls;
    if (mock.unload_error) return mock.unload_error;
    mock.loaded = 0; return 0;
}
int sceIoWrite(int fd, const void* data, unsigned size)
{
    (void)fd;
    if (mock.log_error) return mock.log_error;
    assert(size < sizeof(mock.log) - mock.log_length);
    memcpy(mock.log + mock.log_length, data, size); mock.log_length += size;
    mock.log[mock.log_length] = 0;
    return (int)size;
}
/* These APIs are linked from the bootstrap but excluded from this load test. */
int sceKernelAllocPartitionMemory(int p, const char* n, int t, unsigned s, void* a)
{ (void)p; (void)n; (void)t; (void)s; (void)a; assert(0); return -1; }
void* sceKernelGetBlockHeadAddr(int uid) { (void)uid; assert(0); return NULL; }
int sceKernelFreePartitionMemory(int uid) { (void)uid; assert(0); return -1; }
void* sceKernelFindModuleByName(const char* n) { (void)n; assert(0); return NULL; }
int sceKernelDelayThread(unsigned delay) { (void)delay; assert(0); return -1; }
int sceKernelCreateThread(const char* n, int (*entry)(SceSize, void*), int p, unsigned s, unsigned a, void* o)
{ (void)n; (void)entry; (void)p; (void)s; (void)a; (void)o; assert(0); return -1; }
int sceKernelStartThread(int uid, unsigned s, void* a) { (void)uid; (void)s; (void)a; assert(0); return -1; }
int sceKernelDeleteThread(int uid) { (void)uid; assert(0); return -1; }
int sceKernelWaitThreadEnd(int uid, SceUInt* t) { (void)uid; (void)t; assert(0); return -1; }
int sceIoOpen(const char* p, int f, int m) { (void)p; (void)f; (void)m; assert(0); return -1; }
int sceIoClose(int uid) { (void)uid; assert(0); return -1; }
int main(void)
{
    const char* stage;
    unsigned minimum_total = 0, minimum_max = 0;
    reset();
    assert(load_placement(&stage) == 0 && mock.load_calls == 1 && mock.loaded);
    assert(strstr(mock.log, "PLACEMENT_USER2_LOAD_OK") && strstr(mock.log, "user_consumed=4096 extra_consumed=0"));
    assert(unload_placement() == 0 && !mock.loaded && placement_module == -1);
    assert(strstr(mock.log, "phase=after_unload user_free=864512") && strstr(mock.log, "user_consumed=-4096"));
    reset(); mock.free_before = RA_PLACEMENT2_FREE_FLOOR + RA_PLACEMENT2_MODULE_BUDGET - 1;
    assert(load_placement(&stage) == RA_PLACEMENT2_MEMORY_GUARD && mock.load_calls == 0);
    reset(); mock.consumed = RA_PLACEMENT2_MODULE_BUDGET + 1;
    assert(load_placement(&stage) == RA_PLACEMENT2_MEMORY_GUARD && mock.loaded);
    assert(unload_placement() == 0 && !mock.loaded);
    reset(); mock.bad_segment = 1;
    assert(load_placement(&stage) == -34104 && mock.loaded);
    assert(unload_placement() == 0 && !mock.loaded);
    reset(); mock.query_error = -77;
    assert(load_placement(&stage) == -77 && mock.loaded);
    assert(unload_placement() == 0 && !mock.loaded);
    reset(); mock.load_error = (int)SCE_KERNEL_ERROR_PARTITION_MISMATCH;
    assert(load_placement(&stage) == mock.load_error && !mock.loaded);
    assert(strcmp(stage, "load_placement") == 0);
    assert(unload_placement() == 0 && mock.unload_calls == 0);
    reset(); assert(load_placement(&stage) == 0); mock.partition_error = -77;
    assert(unload_placement() == -77 && !mock.loaded && mock.unload_calls == 1);
    reset(); assert(load_placement(&stage) == 0); mock.unload_error = -88;
    assert(unload_placement() == -88 && mock.loaded && placement_module == 42);
    mock.unload_error = 0;
    assert(unload_placement() == 0 && !mock.loaded && mock.unload_calls == 2);
    reset(); mock.log_error = -77;
    assert(load_placement(&stage) < 0 && mock.load_calls == 0);
    reset(); mock.fail_log_after_load = 1;
    assert(load_placement(&stage) < 0 && mock.loaded);
    assert(unload_placement() < 0 && !mock.loaded && mock.unload_calls == 1);
    reset(); stopping = 1;
    assert(load_placement(&stage) == -34001 && mock.load_calls == 0);
    reset(); assert(load_placement(&stage) == 0);
    mock.consumed = mock.free_before - RA_PLACEMENT2_FREE_FLOOR + 1;
    assert(sample_memory(20, &minimum_total, &minimum_max) == RA_PLACEMENT2_MEMORY_GUARD);
    assert(unload_placement() == 0 && !mock.loaded);
    puts("PASS real lite load path: partition2 options, measurements, memory guards, failed load/query/log, unload and retry");
    return 0;
}
