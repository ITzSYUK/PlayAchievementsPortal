#ifndef RA_LITE_TEST_SDK_H
#define RA_LITE_TEST_SDK_H
/* Host-only API doubles for exercising the real load-only bootstrap. */
#include <stddef.h>
#include <stdint.h>
typedef int SceUID;
typedef unsigned SceSize;
typedef unsigned SceUInt;
typedef struct { int64_t st_size; } SceIoStat;
#define PSP_MODULE_INFO(name, attr, major, minor)
#define PSP_MODULE_KERNEL 0x1000
#define PSP_SMEM_High 1
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_TRUNC 4
#define SCE_KERNEL_ERROR_PARTITION_MISMATCH UINT32_C(0x80020132)
#define SCE_KERNEL_ERROR_NOT_STARTED UINT32_C(0x80020134)
typedef struct { unsigned size, startaddr, memsize, attr; } PspSysmemPartitionInfo;
typedef struct { char modname[28]; int modid, mem_id; void* stub_top; unsigned stub_size; } SceModule;
int sceKernelQueryMemoryInfo(unsigned,int*,int*);
int sceKernelGetModuleIdList(int*, unsigned, int*);
SceModule* sceKernelFindModuleByUID(int);
typedef struct { unsigned size; int mpidtext, mpiddata; unsigned flags; char position, access, reserved[2]; } SceKernelLMOption;
typedef struct { unsigned size; int mpidstack; unsigned stacksize; int priority; unsigned attribute; } SceKernelSMOption;
#define PSP_THREAD_ATTR_USER 0x80000000u
#define PSP_SMEM_Low 0
typedef struct {
    unsigned size, attribute, text_addr, text_size, data_size, bss_size;
    unsigned nsegment, segmentaddr[4], segmentsize[4], entry_addr, gp_value;
    char name[28];
} SceKernelModuleInfo;
int sceKernelQueryMemoryPartitionInfo(int, PspSysmemPartitionInfo*);
unsigned sceKernelPartitionTotalFreeMemSize(int);
unsigned sceKernelPartitionMaxFreeMemSize(int);
int sceKernelAllocPartitionMemory(int, const char*, int, unsigned, void*);
void* sceKernelGetBlockHeadAddr(int);
int sceKernelFreePartitionMemory(int);
int sceKernelLoadModule(const char*, int, SceKernelLMOption*);
int sceKernelQueryModuleInfo(int, SceKernelModuleInfo*);
int sceKernelUnloadModule(int);
int sceKernelStartModule(int, unsigned, void*, int*, SceKernelSMOption*);
int sceKernelStopModule(int, unsigned, void*, int*, SceKernelSMOption*);
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int);
void sceKernelDcacheWritebackInvalidateRange(const void*, unsigned);
void* sceKernelFindModuleByName(const char*);
int sceKernelDelayThread(unsigned);
int sceKernelCreateThread(const char*, int (*)(SceSize, void*), int, unsigned, unsigned, void*);
int sceKernelStartThread(int, unsigned, void*);
int sceKernelDeleteThread(int);
int sceKernelGetThreadId(void);
int sceKernelWaitThreadEnd(int, SceUInt*);
/* Fields used by the stop helper; the production build uses the SDK type. */
typedef struct { SceSize size; int status, exitStatus; } SceKernelThreadInfo;
#define PSP_THREAD_STOPPED 16
#define PSP_THREAD_KILLED 32
int sceKernelReferThreadStatus(int, SceKernelThreadInfo*);
int sceIoOpen(const char*, int, int);
int sceIoWrite(int, const void*, unsigned);
int sceIoClose(int);
int sceIoGetstat(const char*, SceIoStat*);
#endif
