#ifndef RA_HTTPS_STOP_TRANSPORT_H
#define RA_HTTPS_STOP_TRANSPORT_H
/* The owner must join before destroying session/heap/network/log state. */
static int stop_transport(void)
{
    SceUInt timeout = 3000000;
    SceKernelThreadInfo info;
    int rc, wait_rc;
    transport_stopping = 1;
    if (transport_thread < 0) return 0;
    if (transport_started) {
        wait_rc = sceKernelWaitThreadEnd(transport_thread, &timeout);
        memset(&info, 0, sizeof(info)); info.size = sizeof(info);
        rc = sceKernelReferThreadStatus(transport_thread, &info);
        if (rc < 0) return rc;
        if (info.status != PSP_THREAD_STOPPED || !transport_done ||
            (wait_rc < 0 && wait_rc != info.exitStatus)) return -35407;
    }
    rc = sceKernelDeleteThread(transport_thread);
    if (rc >= 0) transport_thread = -1;
    return rc;
}
#endif
