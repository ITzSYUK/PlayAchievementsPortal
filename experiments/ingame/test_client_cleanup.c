#define _GNU_SOURCE
#define RA_CLIENT_HOST_TEST 1
#include "../psp/ingame/client/kernel/main.c"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
static ra_client_control shared_test;
static ra_client_control* mapped_control;
static SceModule game_module = { .modname="LocoRoco", .modid=51, .mem_id=80 };
#ifdef RA_CLIENT_HTTPS
static int kirk_error, kirk_calls, foreign;
int sceKernelQueryMemoryInfo(unsigned address,int* partition,int* memory)
{ assert(address==0x0979E4AC);*partition=7;*memory=80;return 0; }
int sceKernelGetModuleIdList(int* ids, unsigned bytes, int* count)
{ assert(bytes == 256 * sizeof(int)); ids[0] = 50; *count = foreign ? 1 : 0; return 0; }
SceModule* sceKernelFindModuleByUID(int uid)
{ static SceModule module = { .modname = "sceNet" }; assert(uid == 50); return &module; }
int sceUtilsBufferCopyWithRange(void* out, int size, void* in, int in_size, int command)
{
    assert(size == 20 && !in && !in_size && command == 14);
    memset(out, ++kirk_calls, 20); return kirk_error;
}
#endif
static SceKernelModuleInfo module_test;
#ifdef RA_CLIENT_HTTPS
static SceKernelModuleInfo game_test;
static int game_query_error;
#endif
static unsigned partition_size;
static int quiet_writer(const void* data, unsigned size, void* context)
{ (void)data; (void)context; return (int)size; }
int sceKernelQueryModuleInfo(int uid, SceKernelModuleInfo* info)
{
#ifdef RA_CLIENT_HTTPS
    if (uid == 51) { *info = game_test; return game_query_error; }
#endif
    assert(uid == 42); *info = module_test; return 0;
}
static struct {
    int stop_error, stop_status, unload_error, free_error, join_error, delete_error;
    int start_error, start_status, loaded, capture;
    int worker_done, stop_diag;
    unsigned starts, stops, unloads, frees, log_length, user_free, seconds;
    int timed_worker;
    unsigned done_second;
    int exit_open_error, exit_write_mode, exit_close_error, exit_reenter, append_error;
    unsigned exit_opens, exit_writes, exit_closes, joins, exit_length;
    char exit_log[2048];
    char log[32768];
} mock;
static void reset(void)
{
    memset(&mock, 0, sizeof(mock)); memset(&shared_test, 0, sizeof(shared_test));
#ifdef RA_CLIENT_HTTPS
    kirk_error = kirk_calls = foreign = 0;
    game_query_error = 0;
    game_uid = -1;
    probe_hook_installed=probe_was_installed=0;
    memset(&game_test, 0, sizeof(game_test)); strcpy(game_test.name, "LocoRoco");
    game_test.text_addr = game_test.segmentaddr[0] = 0x08804000;
    game_test.text_size = 128; game_test.segmentsize[0] = 4096; game_test.nsegment = 1;
#endif
    stopping = 0; control = &shared_test; child = 42; block = 43; start_attempted = 1; thread_id = 44; log_fd = -1;
#ifdef RA_CLIENT_HTTPS
    log_lifecycle=0;log_session=0;
    exit_notice_bytes=0;exit_notice_busy=0;exit_notice_last_result=0;
    exit_log_ready=1;
#endif
    observer_result = 0; partition_size = RA_CLIENT_USER_SIZE;
#ifdef RA_CLIENT_HTTPS
    mock.user_free = 29379072;
#else
    mock.user_free = 864512;
#endif
    memset(&logger, 0, sizeof(logger)); logger.error = -1; /* no log in cleanup tests */
    mock.loaded = 1;
}
int sceKernelCpuSuspendIntr(void) { return 0; }
void sceKernelCpuResumeIntr(int state) { (void)state; }
void sceKernelDcacheWritebackInvalidateRange(const void* address, unsigned size)
{ assert(address == (const void*)(uintptr_t)UINT32_C(0x089B4EE4) && size == 4); }
void sceKernelIcacheInvalidateRange(const void* address,unsigned size)
{ (void)address;assert(size==8); }
int sceKernelStopModule(int uid, unsigned size, void* args, int* status, SceKernelSMOption* option)
{
    assert(uid == 42 && size == 0 && args == NULL && control->stop);
    assert(option->mpidstack == 2 && option->stacksize == RA_CLIENT_ENTRY_STACK);
    assert(option->size == sizeof(*option) && option->priority == 0x60 && option->attribute == 0);
    ++mock.stops;
#ifdef RA_CLIENT_HTTPS
    if (mock.stop_diag) {
        control->owner_thread_id = 37; control->stop_caller_id = 38;
        control->stop_stage = mock.stop_diag;
        control->stop_wait_result = mock.stop_error;
        control->stop_delete_result = RA_STOP_PENDING;
        *status = mock.stop_error;
    }
#endif
    if (mock.stop_error) return mock.stop_error; /* early rejection does not write status */
    *status = mock.stop_status; return uid;
}
int sceKernelUnloadModule(int uid)
{
    assert(uid == 42 && start_attempted == 0); ++mock.unloads;
    if (mock.unload_error) return mock.unload_error;
    mock.loaded = 0; return uid;
}
int sceKernelFreePartitionMemory(int uid)
{
    assert(uid == 43 && child == -1 && control == NULL); ++mock.frees;
    return mock.free_error;
}
int sceKernelWaitThreadEnd(int uid, SceUInt* timeout)
{ assert(uid == 44 && *timeout == 5000000); ++mock.joins; return mock.join_error; }
int sceKernelDeleteThread(int uid) { assert(uid == 44); return mock.delete_error; }
int sceIoWrite(int fd, const void* data, unsigned size)
{
#ifdef RA_CLIENT_HTTPS
    if (fd==2) {
        ++mock.exit_writes;
        if(mock.exit_reenter)assert(exit_notice("nested",0)==-1);
        if(mock.exit_write_mode==2)return 0;
        if(mock.exit_write_mode==3)return -71;
        if(mock.exit_write_mode==4)return (int)size+1;
        if(mock.exit_write_mode==1 && size>7)size=7;
        assert(size<sizeof(mock.exit_log)-mock.exit_length);
        memcpy(mock.exit_log+mock.exit_length,data,size);mock.exit_length+=size;
        mock.exit_log[mock.exit_length]=0;return (int)size;
    }
#endif
    assert(mock.capture && (fd == 1 || fd == 3));
    assert(size < sizeof(mock.log) - mock.log_length);
    memcpy(mock.log + mock.log_length, data, size); mock.log_length += size;
    mock.log[mock.log_length] = 0; return (int)size;
}
int sceIoClose(int fd) { if(fd==2){++mock.exit_closes;return mock.exit_close_error;} return 0; }
int sceKernelQueryMemoryPartitionInfo(int pid, PspSysmemPartitionInfo* info)
{
    assert(pid == 2); info->startaddr = 0x08800000; info->memsize = partition_size;
    info->attr = 0x0F; return 0;
}
unsigned sceKernelPartitionTotalFreeMemSize(int pid)
{ assert(pid == 1 || pid == 2); return pid == 1 ? 903936 : mock.user_free - (mock.loaded ? 274688 : 0); }
unsigned sceKernelPartitionMaxFreeMemSize(int pid) { return sceKernelPartitionTotalFreeMemSize(pid); }
int sceIoGetstat(const char* path, SceIoStat* info)
{ assert(!strcmp(path, RA_CLIENT_DIR "RAClientUser.prx")); info->st_size = 307462; return 0; }
int sceKernelAllocPartitionMemory(int pid, const char* name, int type, unsigned size, void* addr)
{
    assert(pid == 2 && !strcmp(name, "RAClientControl") && type == PSP_SMEM_Low);
    assert(size == sizeof(*control) && addr == NULL); return 43;
}
void* sceKernelGetBlockHeadAddr(int uid) { assert(uid == 43); return mapped_control; }
int sceKernelLoadModule(const char* path, int flags, SceKernelLMOption* option)
{
    assert(!strcmp(path, RA_CLIENT_DIR "RAClientUser.prx") && flags == 0);
    assert(option->mpidtext == 2 && option->mpiddata == 2);
    mock.loaded = 1; return 42;
}
int sceKernelStartModule(int uid, unsigned size, void* args, int* status, SceKernelSMOption* option)
{
    const ra_client_args* request = args;
    assert(uid == 42 && size == sizeof(*request) && request->control == mapped_control);
    assert(control->magic == RA_CLIENT_MAGIC && control->size == sizeof(*control));
    assert(option->size == sizeof(*option) && option->mpidstack == 2);
    assert(option->stacksize == RA_CLIENT_ENTRY_STACK && option->priority == 0x60);
    /* PSP ModuleMgr rejects USER in SMOption before it writes status. */
    assert(option->attribute == 0);
    ++mock.starts;
#ifdef RA_CLIENT_HTTPS
    assert(mock.seconds == 0); /* Start before the observer's first one-second delay. */
    if (mock.timed_worker) {
        control->ready = 1;
#if RA_CLIENT_HOOK_ENABLED
        control->probe_wrapper=module_test.text_addr+64;
#endif
    }
#endif
    if (mock.start_error) return mock.start_error;
    if (mock.worker_done) { control->done = 1; control->result = -35305; }
    *status = mock.start_status; return uid;
}
void* sceKernelFindModuleByName(const char* name)
{ assert(!strcmp(name, "LocoRoco")); return &game_module; }
int sceKernelDelayThread(unsigned us)
{
    if(us==1000)return 0; /* Bounded audit yields do not advance the mock second. */
    if(us==100000 || us==10000)return 0; /* Runtime-hook quiescence. */
    assert(us == 1000000);
#ifdef RA_CLIENT_HTTPS
    if (mock.timed_worker) {
        assert(control && mock.starts == 1);
        assert(control->request_allowed == (mock.seconds >= RA_CLIENT_REQUEST_SECOND));
        if (control->request_allowed && (!mock.done_second || mock.seconds>=mock.done_second)) control->done = 1;
    }
#endif
    ++mock.seconds; return 0;
}
int sceIoOpen(const char* path, int flags, int mode)
{
    if (flags == PSP_O_RDONLY) return (int)UINT32_C(0x80010002);
    if(flags==(PSP_O_WRONLY|PSP_O_APPEND)) {
        assert(!strcmp(path,RA_CLIENT_DIR "client-bootstrap.log") && mode==0);
#ifdef RA_CLIENT_HTTPS
        if(!exit_notice_busy)return mock.append_error ? mock.append_error : 3;
#endif
        ++mock.exit_opens;return mock.exit_open_error ? mock.exit_open_error : 2;
    }
#ifdef RA_CLIENT_HTTPS
    assert((flags & PSP_O_TRUNC) && !(flags & PSP_O_APPEND));
#endif
    assert(!strcmp(path, RA_CLIENT_DIR "client-bootstrap.log")); (void)mode; return 1;
}
int sceIoRead(int fd, void* bytes, unsigned size)
{ (void)fd; (void)bytes; (void)size; assert(0); return -1; }
int sceIoRemove(const char* path)
{ (void)path; return (int)UINT32_C(0x80010002); }
int sceIoRename(const char* source, const char* target)
{ (void)source; (void)target; return (int)UINT32_C(0x80010002); }
unsigned sceKernelGetSystemTimeLow(void) { return mock.seconds*1000000u; }
static void prepare_start(void)
{
    reset(); child = block = -1; control = NULL; start_attempted = 0;
    mock.loaded = 0; mock.capture = 1; written = 0; log_fd = 1;
    ra_probe_log_init(&logger, log_write, NULL);
    memset(&module_test, 0, sizeof(module_test)); strcpy(module_test.name, "RAClientUser");
    module_test.text_addr = module_test.entry_addr = 0x08804000;
    module_test.text_size = 128; module_test.data_size = module_test.bss_size = 1024;
    module_test.nsegment = 1; module_test.segmentaddr[0] = 0x08804000; module_test.segmentsize[0] = 2304;
}
int main(void)
{
    prepare_start(); written=RA_CLIENT_BOOT_LOG_LIMIT;
    assert(ra_probe_log_printf(&logger,"full log is not a session failure\n") && !logger.error);
    assert(written==RA_CLIENT_BOOT_LOG_LIMIT);
    reset(); assert(cleanup() == 0 && child == -1 && block == -1 && control == NULL);
    assert(mock.stops == 1 && mock.unloads == 1 && mock.frees == 1);
    reset(); mock.stop_error = -77;
    assert(cleanup() == -77 && child == 42 && block == 43 && control == &shared_test);
    assert(mock.unloads == 0 && mock.frees == 0);
    mock.stop_error = 0; assert(cleanup() == 0);
    reset(); mock.stop_status = 1;
    assert(cleanup() == -35105 && child == 42 && mock.unloads == 0 && mock.frees == 0);
    reset(); mock.stop_error = (int)SCE_KERNEL_ERROR_NOT_STARTED;
    assert(cleanup() == 0 && child == -1 && block == -1 && mock.unloads == 1 && mock.frees == 1);
    reset(); mock.stop_error = (int)SCE_KERNEL_ERROR_NOT_STARTED; mock.unload_error = -88;
    assert(cleanup() == -88 && child == 42 && block == 43 && control == &shared_test && mock.frees == 0);
    mock.unload_error = 0; assert(cleanup() == 0 && mock.stops == 1);
    reset(); mock.unload_error = -88;
    assert(cleanup() == -88 && child == 42 && start_attempted == 0 && block == 43);
    mock.unload_error = 0; assert(cleanup() == 0 && mock.stops == 1 && mock.unloads == 2);
    reset(); mock.free_error = -99;
    assert(cleanup() == -99 && child == -1 && block == 43 && control == NULL);
    mock.free_error = 0; assert(cleanup() == 0 && mock.frees == 2);
    reset(); mock.join_error = -66;
    assert(module_stop(0, NULL) == -66 && child == 42 && mock.stops == 0 && mock.frees == 0);
    mock.join_error = 0; assert(module_stop(0, NULL) == 0 && thread_id == -1);
    reset(); mock.delete_error = -55;
    assert(module_stop(0, NULL) == -55 && child == -1 && block == -1 && thread_id == 44);
    mock.delete_error = 0; assert(module_stop(0, NULL) == 0 && thread_id == -1);
    reset(); start_attempted = 0; assert(cleanup() == 0 && mock.stops == 0);
    reset(); ra_probe_log_init(&logger, quiet_writer, NULL);
    memset(&module_test, 0, sizeof(module_test)); strcpy(module_test.name, "RAClientUser");
    module_test.text_addr = module_test.entry_addr = 0x08804000;
    module_test.text_size = 128; module_test.data_size = module_test.bss_size = 1024;
    module_test.nsegment = 1; module_test.segmentaddr[0] = 0x08804000; module_test.segmentsize[0] = 2304;
    assert(validate_child() == 0);
    module_test.segmentsize[0] = RA_CLIENT_MODULE_LIMIT;
    assert(validate_child() == 0);
    module_test.segmentsize[0] = RA_CLIENT_MODULE_LIMIT + 1;
    assert(validate_child() == -35104);
#ifdef RA_CLIENT_HTTPS
    module_test.text_size = 330820; module_test.data_size = 1788; module_test.bss_size = 11380;
    module_test.segmentsize[0] = 344332; /* actual relocated 0.38 module */
    assert(validate_child() == 0);
#endif
    module_test.text_size = 128; module_test.data_size = module_test.bss_size = 1024;
    module_test.segmentsize[0] = 2304;
    module_test.entry_addr = 0x0880407Du; assert(validate_child() == -35104);
    module_test.entry_addr = 0x88804000u; assert(validate_child() == -35104);
    module_test.entry_addr = 0x08805000u; assert(validate_child() == -35104);
    module_test.entry_addr = module_test.text_addr;
    module_test.segmentaddr[0] = 0x08808000; assert(validate_child() == -35104);
    module_test.segmentaddr[0] = RA_CLIENT_USER_BASE + RA_CLIENT_USER_SIZE; assert(validate_child() == -35104);
#ifdef RA_CLIENT_HTTPS
    /* Relocated text in the extended portion is valid; crossing its end is not. */
    module_test.text_addr = module_test.entry_addr = module_test.segmentaddr[0] = 0x0A100000;
    assert(validate_child() == 0);
    module_test.text_addr = module_test.entry_addr = module_test.segmentaddr[0] = 0x0AFFFFC0;
    assert(validate_child() == -35104);
    assert(ra_client_user_range(0x0AFFFFFC, 4) && !ra_client_user_range(0x0AFFFFFC, 5));
    assert(!ra_client_user_range(0xFFFFFFFF, 2) && !ra_client_user_range(0x88800000, 4));
#endif
    /* Keep the actual pointer inside PSP user range without replacing mappings. */
    mapped_control = mmap((void*)0x08900000, 4096, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(mapped_control != MAP_FAILED && (uintptr_t)mapped_control == 0x08900000);
#ifdef RA_CLIENT_HTTPS
    {
        uint32_t* code=mmap((void*)0x08804000,4096,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        assert(code!=MAP_FAILED);
        prepare_start();control=mapped_control;memset(control,0,sizeof(*control));
        code[0]=0x0C000000 | (0x08804280>>2);
        code[128]=0x08804230;code[130]=0x00010005;
        code[131]=0x08804220;code[132]=0x08804280;code[136]=0x289D82FE;
        memcpy((unsigned char*)code+560,"sceDisplay",11);
        game_module.stub_top=(void*)0x08804200;game_module.stub_size=20;
        assert(publish_game_map()==0);
        assert(strstr(mock.log,"GAME_TABLE_PROBE name=input_descriptor") && strstr(mock.log,"guard=1 result=-1"));
#if RA_CLIENT_HOOK_ENABLED
        assert(strstr(mock.log,"GAME_RUNTIME_GUARD table=-1 loop=-1") && strstr(mock.log,"same_game_block=1"));
#else
        assert(strstr(mock.log,"GAME_TABLE_DONE descriptor_result=-1") && strstr(mock.log,"same_game_block=1"));
#endif
        game_module.stub_top=NULL;game_module.stub_size=0;control=NULL;
        assert(munmap(code,4096)==0);
    }
    {
        uint32_t* code=mmap((void*)0x08840000,65536,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        uint32_t* descriptor_page=mmap((void*)0x089B4000,4096,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        uint32_t* dispatch_page=mmap((void*)0x0897C000,4096,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        uint32_t* descriptor;
        assert(code!=MAP_FAILED && descriptor_page!=MAP_FAILED && dispatch_page!=MAP_FAILED);
        prepare_start();control=mapped_control;memset(control,0,sizeof(*control));
        control->game_map.count=3;control->game_map.regions[0].address=0x08840000;
        control->game_map.regions[0].size=65536;
        control->game_map.regions[1].address=0x089B4000;control->game_map.regions[1].size=4096;
        control->game_map.regions[2].address=0x0897C000;control->game_map.regions[2].size=4096;
        game_test.text_addr=0x08840000;game_test.text_size=65536;
        descriptor=descriptor_page+0xED0/4;
        descriptor[0]=0x12345678;descriptor[1]=0;descriptor[2]=0x08841480;
        descriptor[3]=0x089B4F00;descriptor[4]=0xDEADBEEF;descriptor[5]=0x08842D90;
        dispatch_page[0x21C/4]=0x08841480;dispatch_page[0x220/4]=0x08841500;
        code[0x3F0/4]=0x27BDFFE0;code[0x3F4/4]=0xAFBF001C;
        code[0x400/4]=0x3C04089B;code[0x404/4]=0x24854ED0;
        code[0x408/4]=0x8C824EE4;code[0x40C/4]=0x0C000000|(0x08841480>>2);
        code[0x500/4]=0x3C030898;code[0x504/4]=0x2463C21C;
        code[0x600/4]=0x8E190010;code[0x604/4]=0x8F390014;code[0x608/4]=0x0320F809;
        code[0x4440/4]=0x27BDFFF0;code[0x4444/4]=0x0C000000|(0x08845000>>2);
        code[0x5000/4]=0x08000000|(0x08844440>>2); /* Cycle must not grow queue. */
        code[0x2CA0/4]=0x0E2105E2;code[0x2CA8/4]=0x0E211110;
        code[0x1880/4]=0x3C03089D;
#if RA_CLIENT_HOOK_ENABLED
        child=42;
        control->probe_wrapper=0x08804040;
        assert(install_runtime_probe()==0 && probe_hook_installed && control->probe_installed);
        assert(descriptor[5]==control->probe_wrapper);
        assert(remove_runtime_probe()==0 && !probe_hook_installed && !control->probe_installed);
        assert(descriptor[5]==0x08842D90);
        descriptor[5]=0x08804044;probe_hook_installed=1;control->probe_installed=1;
        assert(remove_runtime_probe()==-35409 && probe_hook_installed);
        probe_hook_installed=0;control->probe_installed=0;descriptor[5]=0x08842D90;
        code[0x2CA0/4]=0;assert(install_runtime_probe()==-35409 && descriptor[5]==0x08842D90);
        code[0x2CA0/4]=0x0E2105E2;
        child=-1;
#endif
        audit_table_flow(&game_test);
        assert(strstr(mock.log,"GAME_TABLE_FIELD name=input_descriptor index=2 kind=code address=0x08841480"));
        assert(strstr(mock.log,"GAME_PAUSE_NODE index=0 address=0x08844440 depth=0 span=1024"));
        assert(strstr(mock.log,"GAME_PAUSE_NODE index=3 address=0x08845000 depth=1 span=384"));
        assert(strstr(mock.log,"GAME_PAUSE_DONE guard_result=0 result=0 nodes=4"));
        assert(strstr(mock.log,"GAME_TABLE_DONE descriptor_result=0 pause_result=0"));
        assert(!strstr(mock.log,"dispatch_candidate"));
        assert(!strstr(mock.log,"12345678") && !strstr(mock.log,"DEADBEEF") && !strstr(mock.log,"address=0x089B4F00"));
        {
            ra_pause_flow capped={{{0,0,0}},0,0,0,0,0};unsigned node;
            for(node=0;node<20;++node)pause_add(&capped,&game_test,0x08848000+node*512,1,384);
            assert(capped.count==RA_PAUSE_NODES && capped.dropped==8);
            pause_add(&capped,&game_test,0x08848004,2,384); /* Covered span: no duplicate. */
            pause_add(&capped,&game_test,0x0979E4AC,1,384); /* Outside text: never read. */
            assert(capped.count==RA_PAUSE_NODES && capped.dropped==8);
            assert(pause_signature(&game_test,0x089B4EE4,0x08842D90)==-1);
            {
                ra_table_audit_state cancelled_state={RA_TABLE_CODE_BUDGET,0,{0}};
                control->stop=1;
                pause_node(&capped,&game_test,0,&cancelled_state);
                assert(capped.errors==1); /* Cancellation cannot look like a complete node. */
                control->stop=0;
            }
        }
        code[0x4440/4]=0;mock.log_length=0;mock.log[0]=0;
        audit_table_flow(&game_test);
        assert(strstr(mock.log,"GAME_PAUSE_GUARD pc=0x08844440 result=-2") && !strstr(mock.log,"CODE "));
        code[0x4440/4]=0x27BDFFF0;
        descriptor[5]=0;mock.log_length=0;mock.log[0]=0;
        audit_table_flow(&game_test);
        assert(strstr(mock.log,"guard=1 result=-2") && !strstr(mock.log,"GAME_TABLE_FIELD"));
        control=NULL;
        assert(munmap(code,65536)==0 && munmap(descriptor_page,4096)==0 && munmap(dispatch_page,4096)==0);
    }
#endif
    prepare_start(); assert(start_client(RA_CLIENT_LOAD_SECOND) == 0 && mock.starts == 1);
#ifdef RA_CLIENT_HTTPS
    assert(control->entropy_status == 0 && kirk_calls == 4);
#endif
    assert(strstr(mock.log, "client_sm_option stack_partition=2 stack_bytes=8192 priority=96 attribute=0x00000000"));
    assert(cleanup() == 0);
    prepare_start(); mock.start_error = (int)UINT32_C(0x80020001);
    mock.stop_error = (int)SCE_KERNEL_ERROR_NOT_STARTED;
    assert(observer(0, NULL) == 0 && observer_result == mock.start_error && mock.starts == 1);
    assert(mock.stops == 1 && mock.unloads == 1 && mock.frees == 1);
    assert(strstr(mock.log, "client start status: 0xFFFFFFFF"));
#ifdef RA_CLIENT_HTTPS
    assert(control==NULL && mapped_control->log_session==1);
    assert(strstr(mock.log,"LOG_SESSION id=1 component=bootstrap"));
    assert(strstr(mock.log,"stage=user_start_begin") < strstr(mock.log,"stage=user_start_end result=-2147352575"));
    assert(strstr(mock.log,"stage=bootstrap_cleanup_begin") < strstr(mock.log,"stage=bootstrap_cleanup_end result=0"));
    assert(strstr(mock.log, "memory phase=after_cleanup second=0 user_free=29379072"));
#else
    assert(strstr(mock.log, "memory phase=after_cleanup second=10 user_free=864512"));
#endif
    assert(strstr(mock.log, "result_hex=0x80020001 cleanup_result=0"));
    assert(!strstr(mock.log, "second=120"));
    prepare_start(); mock.start_error = -77; mock.stop_error = -88;
    assert(observer(0, NULL) == 0 && observer_result == -88 && mock.unloads == 0 && mock.frees == 0);
    assert(child == 42 && block == 43 && control == mapped_control && control->stop);
    mock.stop_error = (int)SCE_KERNEL_ERROR_NOT_STARTED; assert(module_stop(0, NULL) == 0);
#ifdef RA_CLIENT_HTTPS
    {
        ra_client_control* low_control = mapped_control;
        mapped_control = mmap((void*)0x0A100000, 4096, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        assert(mapped_control != MAP_FAILED && (uintptr_t)mapped_control == 0x0A100000);
        prepare_start(); assert(start_client(RA_CLIENT_LOAD_SECOND) == 0 && control == mapped_control && mock.starts == 1);
        assert(cleanup() == 0 && munmap(mapped_control, 4096) == 0);
        mapped_control = low_control;
    }
    prepare_start(); partition_size = 0x01800000;
    assert(observer(0, NULL) == 0 && observer_result == -35102);
    assert(mock.starts == 0 && mock.frees == 0 && child == -1 && block == -1);
    assert(strstr(mock.log, "USER_LAYOUT_UNSUPPORTED start=0x08800000 size=25165824"));
    prepare_start(); partition_size = 0x03700000;
    assert(start_client(0) == -35102 && mock.starts == 0 && block == -1);
    prepare_start(); mock.user_free = 864512;
    assert(observer(0, NULL) == 0 && observer_result == -35316);
    assert(mock.starts == 0 && mock.frees == 0 && child == -1 && block == -1);
    assert(strstr(mock.log, "EARLY_WINDOW_MISSED") && !strstr(mock.log, "allocate client control"));
    {
        uint32_t* runtime_code=mmap((void*)0x08840000,65536,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        uint32_t* runtime_table=mmap((void*)0x089B4000,4096,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        uint32_t* descriptor;
        assert(runtime_code!=MAP_FAILED && runtime_table!=MAP_FAILED);
        prepare_start();mock.timed_worker=1;
        game_test.text_addr=game_test.segmentaddr[0]=0x08840000;game_test.text_size=65536;
        game_test.segmentsize[0]=65536;game_test.segmentaddr[1]=0x089B4000;
        game_test.segmentsize[1]=4096;game_test.nsegment=2;
        descriptor=runtime_table+0xED0/4;descriptor[5]=0x08842D90;
        runtime_code[0x2CA0/4]=0x0E2105E2;
        assert(observer(0,NULL)==0 && observer_result==0);
        assert(mock.starts==1 && mock.seconds==RA_CLIENT_REQUEST_SECOND+1 && mock.frees==0);
        assert(mock.unloads==0 && mock.stops==0 && child==42 && block==43);
        assert(strstr(mock.log,"RUNTIME_PROBE_RETAIN user=RESIDENT control=RESIDENT"));
        assert(descriptor[5]==0x08842D90 && !probe_hook_installed);
        assert(strstr(mock.log,"memory phase=before_load second=0"));
        assert(strstr(mock.log,"RUNTIME_PROBE_INSTALL") && strstr(mock.log,"RUNTIME_PROBE_RESTORE"));
        assert(strstr(mock.log,"HTTPS_REQUEST_RELEASE second=10 network_ready=1"));
        assert(mapped_control->game_map.count==2 && mapped_control->game_map.regions[0].address==0x08840000);
        /* A live worker must survive the old bootstrap 720-second watchdog. */
        prepare_start();mock.timed_worker=1;mock.done_second=RA_CLIENT_OBSERVE_SECONDS+120;
        game_test.text_addr=game_test.segmentaddr[0]=0x08840000;game_test.text_size=65536;
        game_test.segmentsize[0]=65536;game_test.segmentaddr[1]=0x089B4000;
        game_test.segmentsize[1]=4096;game_test.nsegment=2;
        descriptor[5]=0x08842D90;
        assert(observer(0,NULL)==0 && observer_result==0);
        assert(mock.seconds==mock.done_second+1 && mock.seconds>RA_CLIENT_OBSERVE_SECONDS);
        assert(descriptor[5]==0x08842D90 && !probe_hook_installed);
        control=mapped_control;assert(game_map_live()==0);
        game_test.segmentaddr[0]+=4;assert(game_map_live()==-35409);game_test.segmentaddr[0]-=4;
        game_query_error=-55;assert(game_map_live()==-55);control=NULL;
        assert(munmap(runtime_code,65536)==0 && munmap(runtime_table,4096)==0);
    }
    prepare_start(); game_query_error = -44; mock.timed_worker = 1;
    assert(observer(0, NULL) == 0 && observer_result == -44 && mock.frees == 1);
    assert(!strstr(mock.log, "HTTPS_REQUEST_RELEASE"));
    prepare_start(); game_test.attribute = PSP_MODULE_KERNEL; mock.timed_worker = 1;
    assert(observer(0, NULL) == 0 && observer_result == -35409 && mock.frees == 1);
    assert(!strstr(mock.log, "HTTPS_REQUEST_RELEASE"));
    prepare_start(); game_test.segmentaddr[0] = 0x0A000000; mock.timed_worker = 1;
    assert(observer(0, NULL) == 0 && observer_result == -35409 && mock.frees == 1);
    prepare_start(); kirk_error = -1;
    assert(start_client(0) == -35302 && child == -1 && mock.starts == 0);
    assert(cleanup() == 0 && mock.frees == 1 && mapped_control->entropy[0] == 0);
    prepare_start(); foreign = 1;
    assert(start_client(0) == -35301 && mock.starts == 0 && kirk_calls == 0);
    assert(cleanup() == 0 && mock.frees == 1);
    prepare_start(); mock.worker_done = 1; mock.stop_diag = RA_STOP_WAIT_ERROR;
    mock.stop_error = (int)UINT32_C(0x800200D2);
    assert(observer(0, NULL) == 0 && observer_result == mock.stop_error && child == 42 && block == 43 && control == mapped_control);
    assert(mock.unloads == 0 && mock.frees == 0);
    assert(strstr(mock.log, "client_stop_diag stage=WAIT_ERROR(3) owner=0x00000025 caller=0x00000026 wait=0x800200D2 delete=0x7FFFFFFF"));
    assert(strstr(mock.log, "client_bootstrap_finished result=-35305"));
    mock.stop_error = mock.stop_diag = 0;
    assert(module_stop(0, NULL) == 0 && child == -1 && block == -1);
#endif
    assert(munmap(mapped_control, 4096) == 0);
#ifdef RA_CLIENT_HTTPS
    reset();mock.capture=1;log_fd=1;written=RA_CLIENT_BOOT_LOG_LIMIT-RA_CLIENT_EXIT_LOG_RESERVE-RA_LOG_STOP_RESERVE;
    ra_probe_log_init(&logger,log_write,NULL);log_session=9;
    assert(ra_probe_log_printf(&logger,"ordinary diagnostics dropped\n"));
    assert(mock.log_length==0);
    log_stage("bootstrap_cleanup_end",-99);
    assert(strstr(mock.log,"LIFECYCLE session=9 uptime_us32=0 stage=bootstrap_cleanup_end result=-99"));
    assert(written<=RA_CLIENT_BOOT_LOG_LIMIT);
    written=RA_CLIENT_BOOT_LOG_LIMIT;log_lifecycle=1;
    { unsigned length=mock.log_length;log_stage("full",0);assert(mock.log_length==length && !logger.error); }
    /* Reboot notification survives a failed/closed normal logger, never waits
     * for suspended users or calls cleanup/ModuleMgr. Flags distinguish the
     * user notification from worker cleanup completion. */
    reset();log_session=7;shared_test.magic=RA_CLIENT_MAGIC;shared_test.size=sizeof(shared_test);
    shared_test.exit_user_flags=RA_EXIT_USER_REBOOT;logger.write=NULL;logger.error=-88;
    assert(module_reboot_phase(1,NULL,0,0)==0 && module_reboot_phase(2,NULL,0,0)==0);
    assert(module_reboot_phase(99,NULL,0,0)==0 && !mock.exit_opens && !mock.joins);
    mock.exit_write_mode=1;mock.exit_reenter=1;
    assert(module_reboot_phase(3,NULL,0,0)==0);
    assert(strstr(mock.exit_log,"EXIT_NOTICE session=7") && strstr(mock.exit_log,"source=kernel_reboot_phase3"));
    assert(strstr(mock.exit_log,"user_flags=1 worker_done=0") && mock.exit_writes>1);
    assert(mock.exit_opens==1 && mock.exit_closes==1);
    assert(!mock.joins && !mock.stops && !mock.unloads && !mock.frees && control==&shared_test);
    assert(!exit_notice_busy && !exit_notice_last_result && logger.error==-88);
    reset();assert(module_reboot_phase(3,NULL,0,0)==0 && !mock.exit_opens);
    reset();log_session=8;mock.exit_open_error=-72;
    assert(module_reboot_phase(3,NULL,0,0)==0 && exit_notice_last_result==-72 && !mock.exit_closes);
    reset();log_session=8;mock.exit_write_mode=2;
    assert(module_reboot_phase(3,NULL,0,0)==0 && exit_notice_last_result==-1 && mock.exit_closes==1);
    reset();log_session=8;mock.exit_write_mode=3;
    assert(module_reboot_phase(3,NULL,0,0)==0 && exit_notice_last_result==-71 && mock.exit_closes==1);
    reset();log_session=8;mock.exit_write_mode=4;
    assert(module_reboot_phase(3,NULL,0,0)==0 && exit_notice_last_result==-1 && mock.exit_closes==1);
    reset();log_session=8;mock.exit_close_error=-73;
    assert(module_reboot_phase(3,NULL,0,0)==0 && exit_notice_last_result==-73);
    prepare_start();mock.append_error=-89;
    assert(observer(0,NULL)==0 && observer_result==-89 && !mock.starts && !exit_log_ready);
    assert(strstr(mock.log,"LOG_SESSION id=1 component=bootstrap") && strstr(mock.log,"open bootstrap append: 0xFFFFFFA7"));
    assert(!mock.exit_opens && !mock.unloads && !mock.frees);
    reset();log_session=9;mock.join_error=-66;
    assert(module_stop(0,NULL)==-66 && mock.joins==1);
    assert(strstr(mock.exit_log,"source=kernel_module_stop_enter result=0") &&
        strstr(mock.exit_log,"source=kernel_module_stop_wait result=-66"));
    assert(!mock.stops && !mock.unloads && !mock.frees);
    reset();log_session=10;written=RA_CLIENT_BOOT_LOG_LIMIT-RA_CLIENT_EXIT_LOG_RESERVE;
    mock.capture=1;log_fd=1;log_lifecycle=1;ra_probe_log_init(&logger,log_write,NULL);
    log_stage("ordinary_stop_limit",0);assert(!mock.log_length && !logger.error);
    for(unsigned i=0;i<30;++i)assert(module_reboot_phase(3,NULL,0,0)==0);
    assert(exit_notice_bytes<=RA_CLIENT_EXIT_LOG_RESERVE && mock.exit_length<=exit_notice_bytes);
    assert(written+exit_notice_bytes<=RA_CLIENT_BOOT_LOG_LIMIT && mock.exit_opens<30);
    puts("PASS exit notices: reboot/early module_stop, no ModuleMgr/join in reboot, independent append, partial/invalid I/O, reentry, 128KiB budget");
#endif
    puts("PASS client lifecycle: SMOption privilege, failed start/NOT_STARTED cleanup, early timestamp, retain/retry on uncertain stop");
    return 0;
}
