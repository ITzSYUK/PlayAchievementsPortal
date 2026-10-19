#include "probe_log.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RA_CLIENT_DIR "mock:/"
#define PSP_O_WRONLY 2
#define PSP_O_APPEND 256
static struct { int result;unsigned stop,resume_stage; } ctl,*control=&ctl;
static ra_probe_log logger;
static int log_fd=3,open_failures,write_failure,old_close_error;
static unsigned sleep_epoch=1,sleep_flags=0x80020000,opens,closes;
static uint64_t clock_ms=1000,log_sync_next;
static char output[2048]="OLD SESSION\n";
static unsigned used=12;
static uint64_t now_ms(void* p){(void)p;return clock_ms;}
static int sceIoOpen(const char* path,int flags,int mode)
{
    assert(!strcmp(path,"mock:/client-user.log") && flags==(PSP_O_WRONLY|PSP_O_APPEND) && !mode);
    ++opens;if(open_failures){--open_failures;return -4;}return 9;
}
static int sceIoClose(int fd)
{ assert(fd==3 || fd==9);++closes;return fd==3?old_close_error:0; }
static int write_log(const void* bytes,unsigned size,void* p)
{
    unsigned count=size>7?7:size;(void)p;
    assert(log_fd==9);if(write_failure)return -8;
    assert(used+count<sizeof(output));memcpy(output+used,bytes,count);used+=count;output[used]=0;
    return (int)count;
}
#include "../psp/ingame/https/user/resume_log.h"
int main(void)
{
    unsigned before;
    ra_probe_log_init(&logger,write_log,NULL);logger.error=-77;
    open_failures=2;old_close_error=-9;
    assert(!resume_log_prepare() && opens==1 && closes==1 && log_fd==-1 && !ctl.stop);
    clock_ms=1099;assert(!resume_log_prepare() && opens==1);
    clock_ms=1100;assert(!resume_log_prepare() && opens==2);
    clock_ms=1200;assert(resume_log_prepare() && opens==3 && log_fd==9 && !logger.error);
    assert(!strncmp(output,"OLD SESSION\n",12) && strstr(output,"prior_log_error=-77 old_close=-9"));
    assert(log_sync_next==6200 && ctl.resume_stage==1);
    before=opens;assert(resume_log_prepare() && opens==before);
    ++sleep_epoch;write_failure=1;
    assert(!resume_log_prepare() && log_fd==-1 && !ctl.stop && logger.error==-8);
    write_failure=0;clock_ms+=100;assert(resume_log_prepare() && !logger.error);
    ++sleep_epoch;open_failures=100;
    assert(!resume_log_prepare() && !ctl.stop);
    clock_ms+=5000;assert(!resume_log_prepare() && ctl.stop && ctl.result==-4);
    puts("PASS resume log: append-only existing session, delayed media retries, invalid old handle, stale/write error recovery, bounded failure and no truncation");
    return 0;
}
