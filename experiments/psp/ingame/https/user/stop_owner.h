#ifndef RA_HTTPS_STOP_OWNER_H
#define RA_HTTPS_STOP_OWNER_H
/* Called by LoadExec while ModuleMgr may hold its mutex. Signal only: an
 * owner join could deadlock on a module operation in startup/network cleanup. */
static int https_reboot_before(void)
{
    ra_client_control* shared = control;
    if (shared) {
        __atomic_fetch_or(&shared->exit_user_flags, RA_EXIT_USER_REBOOT, __ATOMIC_RELEASE);
        __atomic_store_n(&shared->stop, 1, __ATOMIC_RELEASE);
    }
    return 0;
}
/* Included by the PSP worker and a host failure-injection test. Uses only
 * ThreadMan and the live control block, never libc or the closed log. */
static int https_stop_owner(void)
{
    SceUInt timeout = 3000000;
    ra_client_control* shared = control;
    SceKernelThreadInfo info;
    int rc, wait_rc;
    if (!shared) return thread_id < 0 ? 0 : -35312;
    __atomic_fetch_or(&shared->exit_user_flags, RA_EXIT_USER_STOP, __ATOMIC_RELEASE);
    shared->stop = 1;
    shared->owner_thread_id = thread_id;
    shared->stop_caller_id = sceKernelGetThreadId();
    shared->stop_wait_result = shared->stop_delete_result = RA_STOP_PENDING;
    shared->stop_refer_result = RA_STOP_PENDING;
    shared->stop_thread_status = shared->stop_exit_status = 0;
    shared->stop_timeout_us = timeout;
    shared->stop_stage = RA_STOP_ENTER;
    if (thread_id < 0) { shared->stop_stage = RA_STOP_NO_THREAD; return 0; }
    shared->stop_stage = RA_STOP_WAIT;
    rc = sceKernelWaitThreadEnd(thread_id, &timeout);
    shared->stop_wait_result = rc;
    shared->stop_timeout_us = timeout;
    wait_rc = rc;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    rc = sceKernelReferThreadStatus(thread_id, &info);
    shared->stop_refer_result = rc;
    if (rc < 0) { shared->stop_stage = RA_STOP_REFER_ERROR; return rc; }
    shared->stop_thread_status = info.status;
    shared->stop_exit_status = info.exitStatus;
    /* WaitThreadEnd may return the target's exit status. A negative value
     * alone is not proof of a failed join. Require ThreadMan STOPPED, a
     * matching exit status and the worker's resource-cleanup acknowledgement. */
    if (wait_rc < 0 && (info.status != PSP_THREAD_STOPPED || wait_rc != info.exitStatus)) {
        shared->stop_stage = RA_STOP_WAIT_ERROR; return wait_rc;
    }
    if (info.status != PSP_THREAD_STOPPED) {
        shared->stop_stage = RA_STOP_THREAD_RUNNING; return -35312;
    }
    if (!shared->done) { shared->stop_stage = RA_STOP_NOT_DONE; return -35312; }
    shared->stop_stage = RA_STOP_DELETE;
    rc = sceKernelDeleteThread(thread_id);
    shared->stop_delete_result = rc;
    if (rc < 0) { shared->stop_stage = RA_STOP_DELETE_ERROR; return rc; }
    shared->stop_stage = RA_STOP_FINISHED;
    thread_id = -1; control = NULL;
    return rc;
}
#endif
