#include "lite_test_sdk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SceUID transport_thread;
static volatile int transport_stopping, transport_done;
static int transport_started;
static struct { int wait, refer, status, exit_status, remove; unsigned waits, deletes; } mock;
int sceKernelWaitThreadEnd(int id, SceUInt* timeout)
{ assert(id == 42 && *timeout == 3000000 && transport_stopping); ++mock.waits; return mock.wait; }
int sceKernelReferThreadStatus(int id, SceKernelThreadInfo* info)
{ assert(id == 42 && info->size == sizeof(*info)); info->status = mock.status; info->exitStatus = mock.exit_status; return mock.refer; }
int sceKernelDeleteThread(int id)
{ assert(id == 42 && transport_stopping); ++mock.deletes; return mock.remove; }
#include "../psp/ingame/https/user/stop_transport.h"
static void reset(void)
{
    memset(&mock, 0, sizeof(mock)); transport_thread = 42; transport_started = transport_done = 1;
    transport_stopping = 0; mock.status = PSP_THREAD_STOPPED;
}
int main(void)
{
    reset(); assert(stop_transport() == 0 && transport_thread == -1 && mock.deletes == 1);
    reset(); transport_thread = -1; assert(stop_transport() == 0 && !mock.waits && !mock.deletes);
    reset(); transport_started = transport_done = 0; assert(stop_transport() == 0 && !mock.waits && mock.deletes == 1);
    reset(); mock.wait = -9; assert(stop_transport() == -35407 && transport_thread == 42 && !mock.deletes);
    reset(); mock.wait = mock.exit_status = -9; assert(stop_transport() == 0 && mock.deletes == 1);
    reset(); mock.status = PSP_THREAD_KILLED; assert(stop_transport() == -35407 && !mock.deletes);
    reset(); mock.status = 2; assert(stop_transport() == -35407 && !mock.deletes);
    reset(); transport_done = 0; assert(stop_transport() == -35407 && !mock.deletes);
    reset(); mock.refer = -8; assert(stop_transport() == -8 && !mock.deletes);
    reset(); mock.remove = -7; assert(stop_transport() == -7 && transport_thread == 42);
    mock.remove = 0; assert(stop_transport() == 0 && transport_thread == -1);
    puts("PASS transport stop: joined STOPPED + done required, failed start/delete/refer/wait retain and retry");
    return 0;
}
