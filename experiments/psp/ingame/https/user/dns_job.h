#ifndef RA_DNS_JOB_H
#define RA_DNS_JOB_H
/* One helper per module. Its output and resolver scratch never live on the
 * transport stack. Only transport creates/stops/reaps it; owner reads flags.
 * No resolver deletion, new lookup or module teardown before STOPPED + done. */
typedef struct { unsigned elapsed_ms,stop_calls; int stop_result,result; } ra_dns_report;
static struct {
    int thread,started,rid,result;
    uint32_t done,cancel;
    struct in_addr address;
    char scratch[1024];
} dns_job={.thread=-1};
static int dns_entry(SceSize size,void* args)
{
    (void)size;(void)args;
    dns_job.result=__atomic_load_n(&dns_job.cancel,__ATOMIC_ACQUIRE)?RA_HTTPS_CANCEL:
        sceNetResolverStartNtoA(dns_job.rid,"retroachievements.org",&dns_job.address,1,1);
    __atomic_store_n(&dns_job.done,1,__ATOMIC_RELEASE);
    return 0;
}
/* Nonblocking reap. A failed query/delete retains the UID and dependencies. */
static int ra_dns_reap(void)
{
    SceKernelThreadInfo info;
    int rc;
    if(dns_job.thread<0)return 0;
    if(dns_job.started) {
        memset(&info,0,sizeof(info));info.size=sizeof(info);
        rc=sceKernelReferThreadStatus(dns_job.thread,&info);
        if(rc<0)return rc;
        if(info.status!=PSP_THREAD_STOPPED || !__atomic_load_n(&dns_job.done,__ATOMIC_ACQUIRE))return 1;
    }
    rc=sceKernelDeleteThread(dns_job.thread);
    if(rc<0)return rc;
    __atomic_store_n(&dns_job.thread,-1,__ATOMIC_RELEASE);dns_job.started=0;
    return 0;
}
static int ra_dns_run(int rid,struct in_addr* address,ra_dns_report* report)
{
    uint64_t began=now_ms(NULL),until=began+5000,grace=0,next_stop=0,now;
    int rc,cause=0;
    *report=(ra_dns_report){0,0,0,-35425};
    if(dns_job.thread>=0)return -35424;
    if(until>transport_deadline)until=transport_deadline;
    dns_job.rid=rid;dns_job.result=-35425;
    __atomic_store_n(&dns_job.done,0,__ATOMIC_RELEASE);
    __atomic_store_n(&dns_job.cancel,0,__ATOMIC_RELEASE);
    rc=sceKernelCreateThread("RADnsLookup",dns_entry,0x60,8192,PSP_THREAD_ATTR_USER,NULL);
    if(rc<0)return rc;
    __atomic_store_n(&dns_job.thread,rc,__ATOMIC_RELEASE);
    rc=sceKernelStartThread(dns_job.thread,0,NULL);
    if(rc<0){(void)ra_dns_reap();return rc;}
    dns_job.started=1;
    for(;;) {
        rc=ra_dns_reap();
        if(!rc)break;
        now=now_ms(NULL);
        if(!cause) {
            cause=transport_check();
            if(!cause && now>=until)cause=RA_HTTPS_TIMEOUT;
            if(rc<0 && !cause)cause=rc;
            if(cause){grace=now+1000;__atomic_store_n(&dns_job.cancel,1,__ATOMIC_RELEASE);}
        }
        if(cause) {
            /* Repeat handles Stop racing a helper that has not entered DNS
             * yet. The helper also checks cancel before entering the SDK. */
            if(now>=next_stop) {
                io_enter(IO_DNS_STOP);report->stop_result=sceNetResolverStop(rid);io_leave(IO_DNS_STOP);
                ++report->stop_calls;next_stop=now+100;
            }
            if(now>=grace) {
                /* Transport must stay alive until this job can be reaped.
                 * Its normal bounded owner join then retains module/state. */
                control->result=-35424;control->stop=1;
                report->elapsed_ms=(unsigned)(now_ms(NULL)-began);
                return -35424;
            }
        }
        sceKernelDelayThread(10000);
    }
    report->elapsed_ms=(unsigned)(now_ms(NULL)-began);report->result=dns_job.result;
    if(cause)return cause;
    rc=transport_check();if(rc)return rc;
    if(dns_job.result>=0)*address=dns_job.address;
    return dns_job.result;
}
#endif
