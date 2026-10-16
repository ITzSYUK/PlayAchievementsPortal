#define RA_CLIENT_HTTPS 1
#include "lite_test_sdk.h"
#include "client_shared.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ra_client_control shared;
static ra_client_control* control;
static SceUID thread_id;
static struct { int wait_rc, delete_rc, refer_rc, status, exit_status; unsigned waits, deletes, refers; } mock;
int sceKernelGetThreadId(void) { return 38; }
int sceKernelWaitThreadEnd(int uid, SceUInt* timeout)
{
    assert(uid == 37 && *timeout == 3000000 && shared.stop_stage == RA_STOP_WAIT);
    assert(shared.stop && shared.owner_thread_id == uid && shared.stop_caller_id == 38);
    assert(shared.stop_wait_result == RA_STOP_PENDING && shared.stop_delete_result == RA_STOP_PENDING);
    *timeout = 2900000; ++mock.waits; return mock.wait_rc;
}
int sceKernelDeleteThread(int uid)
{
    assert(uid == 37 && shared.done && shared.stop_stage == RA_STOP_DELETE);
    ++mock.deletes; return mock.delete_rc;
}
int sceKernelReferThreadStatus(int uid, SceKernelThreadInfo* info)
{
    assert(uid == 37 && info->size == sizeof(*info));
    ++mock.refers; info->status = mock.status; info->exitStatus = mock.exit_status;
    return mock.refer_rc;
}
#include "../psp/ingame/https/user/stop_owner.h"
static void reset(void)
{
    memset(&shared, 0, sizeof(shared)); memset(&mock, 0, sizeof(mock));
    control = &shared; thread_id = 37; shared.done = 1;
    shared.result = -35305; /* Worker failure must not prevent safe teardown. */
    mock.status = PSP_THREAD_STOPPED;
}
int main(void)
{
    reset(); assert(https_stop_owner() == 0 && thread_id == -1 && control == NULL);
    assert(shared.stop_stage == RA_STOP_FINISHED && shared.stop_wait_result == 0 && shared.stop_delete_result == 0);
    assert(shared.result == -35305 && shared.stop_timeout_us == 2900000 && mock.waits == 1 && mock.deletes == 1);
    reset(); mock.wait_rc = (int)UINT32_C(0x800200D2);
    assert(https_stop_owner() == mock.wait_rc && thread_id == 37 && control == &shared);
    assert(shared.stop_stage == RA_STOP_WAIT_ERROR && shared.stop_wait_result == mock.wait_rc);
    assert(shared.stop_delete_result == RA_STOP_PENDING && mock.deletes == 0);
    mock.wait_rc = 0; assert(https_stop_owner() == 0 && mock.waits == 2 && mock.deletes == 1);
    reset(); mock.wait_rc = mock.exit_status = (int)UINT32_C(0x800200D2);
    assert(https_stop_owner() == 0 && shared.stop_stage == RA_STOP_FINISHED && mock.deletes == 1);
    assert(shared.result == -35305 && shared.stop_exit_status == mock.exit_status);
    reset(); mock.wait_rc = mock.exit_status = -35305;
    assert(https_stop_owner() == 0 && shared.result == -35305 && mock.deletes == 1);
    reset(); mock.status = 2; /* Ready is not proof of termination, even with done=1. */
    assert(https_stop_owner() == -35312 && shared.stop_stage == RA_STOP_THREAD_RUNNING && mock.deletes == 0);
    reset(); mock.status = PSP_THREAD_KILLED;
    assert(https_stop_owner() == -35312 && mock.deletes == 0);
    reset(); mock.refer_rc = -88;
    assert(https_stop_owner() == -88 && shared.stop_stage == RA_STOP_REFER_ERROR && mock.deletes == 0);
    reset(); mock.delete_rc = (int)UINT32_C(0x800200D2);
    assert(https_stop_owner() == mock.delete_rc && thread_id == 37 && control == &shared);
    assert(shared.stop_stage == RA_STOP_DELETE_ERROR && shared.stop_wait_result == 0 && shared.stop_delete_result == mock.delete_rc);
    mock.delete_rc = 0; assert(https_stop_owner() == 0 && mock.waits == 2 && mock.deletes == 2);
    reset(); shared.done = 0;
    assert(https_stop_owner() == -35312 && shared.stop_stage == RA_STOP_NOT_DONE && mock.deletes == 0);
    assert(thread_id == 37 && control == &shared);
    reset(); thread_id = -1;
    assert(https_stop_owner() == 0 && shared.stop_stage == RA_STOP_NO_THREAD && mock.waits == 0 && mock.deletes == 0);
    reset(); control = NULL;
    assert(https_stop_owner() == -35312 && mock.waits == 0 && mock.deletes == 0);
    thread_id = -1; assert(https_stop_owner() == 0);
    puts("PASS HTTPS stop: negative exit status distinguished from wait failure, ThreadMan STOPPED required, cleanup acknowledgement and retry");
    return 0;
}
