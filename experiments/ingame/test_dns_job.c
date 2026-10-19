#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <netinet/in.h>
#include "ra_https.h"
typedef unsigned SceSize;
typedef struct { unsigned size,status; } SceKernelThreadInfo;
#define PSP_THREAD_STOPPED 16u
#define PSP_THREAD_ATTR_USER 1
#define IO_DNS_STOP 13
static struct { int result,stop; } test_control,*control=&test_control;
static uint64_t clock_ms,transport_deadline;
static pthread_t helper;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static int released,mode,stops,deletes,create_error,start_error,delete_error,query_error;
static uint32_t helper_stopped;
static int (*entry)(SceSize,void*);
static uint64_t now_ms(void* ignored){(void)ignored;return clock_ms;}
static int transport_check(void){return control->stop?RA_HTTPS_CANCEL:clock_ms>=transport_deadline?RA_HTTPS_TIMEOUT:0;}
static void io_enter(unsigned call){assert(call==IO_DNS_STOP);}
static void io_leave(unsigned call){assert(call==IO_DNS_STOP);}
static int sceNetResolverStartNtoA(int rid,const char* name,struct in_addr* address,unsigned timeout,int retry)
{
    assert(rid==9 && !strcmp(name,"retroachievements.org") && timeout==1 && retry==1);
    pthread_mutex_lock(&mutex);
    if(mode)while(!released)pthread_cond_wait(&wake,&mutex);
    pthread_mutex_unlock(&mutex);
    address->s_addr=UINT32_C(0x01020304);
    return mode?-88:0;
}
static int sceNetResolverStop(int rid)
{
    assert(rid==9);++stops;
    if(mode==2)return -77;
    pthread_mutex_lock(&mutex);released=1;pthread_cond_signal(&wake);pthread_mutex_unlock(&mutex);
    return 0;
}
static void* run(void* ignored)
{
    (void)ignored;entry(0,NULL);
    __atomic_store_n(&helper_stopped,1,__ATOMIC_RELEASE);return NULL;
}
static int sceKernelCreateThread(const char* name,int (*callback)(SceSize,void*),int priority,unsigned stack,unsigned attr,void* ignored)
{
    assert(!strcmp(name,"RADnsLookup") && priority==0x60 && stack==8192 && attr==PSP_THREAD_ATTR_USER && !ignored);
    entry=callback;return create_error?create_error:7;
}
static int sceKernelStartThread(int id,unsigned size,void* args)
{assert(id==7 && !size && !args);return start_error?start_error:pthread_create(&helper,NULL,run,NULL);}
static int sceKernelReferThreadStatus(int id,SceKernelThreadInfo* info)
{
    assert(id==7 && info->size==sizeof(*info));if(query_error)return query_error;
    info->status=__atomic_load_n(&helper_stopped,__ATOMIC_ACQUIRE)?PSP_THREAD_STOPPED:4;return 0;
}
static int sceKernelDeleteThread(int id)
{
    assert(id==7);if(delete_error)return delete_error;
    if(!start_error){assert(__atomic_load_n(&helper_stopped,__ATOMIC_ACQUIRE));assert(!pthread_join(helper,NULL));}
    ++deletes;return 0;
}
static int sceKernelDelayThread(unsigned us)
{
    const struct timespec pause={0,1000000};clock_ms+=us/1000;
    nanosleep(&pause,NULL);return 0;
}
#include "../psp/ingame/https/user/dns_job.h"
static void reset(void)
{
    assert(dns_job.thread<0);memset(&test_control,0,sizeof(test_control));
    clock_ms=0;transport_deadline=30000;released=mode=stops=deletes=0;
    create_error=start_error=delete_error=query_error=0;
    __atomic_store_n(&helper_stopped,0,__ATOMIC_RELEASE);
}
int main(void)
{
    struct in_addr address={0};ra_dns_report report;int rc;
    reset();assert(!ra_dns_run(9,&address,&report) && address.s_addr==UINT32_C(0x01020304) && deletes==1 && !stops);
    reset();mode=1;address.s_addr=0;
    assert(ra_dns_run(9,&address,&report)==RA_HTTPS_TIMEOUT && clock_ms>=5000 && clock_ms<6000);
    assert(stops && deletes==1 && !address.s_addr && !control->stop && report.stop_calls);
    /* A fresh lookup is allowed only after the cancelled helper was reaped. */
    mode=0;released=0;__atomic_store_n(&helper_stopped,0,__ATOMIC_RELEASE);
    assert(!ra_dns_run(9,&address,&report) && deletes==2);
    reset();mode=1;control->stop=1;
    assert(ra_dns_run(9,&address,&report)==RA_HTTPS_CANCEL && deletes==1);
    reset();mode=2;
    assert(ra_dns_run(9,&address,&report)==-35424 && control->stop && !deletes && dns_job.thread==7);
    assert(ra_dns_run(9,&address,&report)==-35424); /* no replacement over live job */
    pthread_mutex_lock(&mutex);released=1;pthread_cond_signal(&wake);pthread_mutex_unlock(&mutex);
    while(!__atomic_load_n(&helper_stopped,__ATOMIC_ACQUIRE))sceKernelDelayThread(10000);
    /* Even a stopped thread cannot be released without its completion ACK. */
    __atomic_store_n(&dns_job.done,0,__ATOMIC_RELEASE);
    assert(ra_dns_reap()==1 && !deletes && dns_job.thread==7);
    __atomic_store_n(&dns_job.done,1,__ATOMIC_RELEASE);
    query_error=-65;assert(ra_dns_reap()==-65 && !deletes && dns_job.thread==7);
    query_error=0;delete_error=-66;assert(ra_dns_reap()==-66 && !deletes && dns_job.thread==7);
    delete_error=0;
    while((rc=ra_dns_reap())!=0){assert(rc==1);sceKernelDelayThread(10000);}
    assert(deletes==1);
    reset();create_error=-55;assert(ra_dns_run(9,&address,&report)==-55 && dns_job.thread<0);
    reset();start_error=-56;assert(ra_dns_run(9,&address,&report)==-56 && deletes==1 && dns_job.thread<0);
    reset();assert(!ra_dns_run(9,&address,&report));
    puts("PASS DNS job: success, deadline Stop/join before retry, cancellation, failed Stop retains live helper, late completion, STOPPED+done required, query/delete/create/start failures");
    return 0;
}
