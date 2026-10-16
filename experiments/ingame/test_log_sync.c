#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int log_fd=1, mock_result;
static uint64_t clock_ms;
static unsigned syncs;
static uint64_t now_ms(void* ignored) { (void)ignored; return clock_ms; }
static int sceIoSync(const char* device,int flags)
{ assert(!strcmp(device,"ms0:") && flags==0);++syncs;clock_ms+=40;return mock_result; }
#include "../psp/ingame/https/user/log_sync.h"
int main(void)
{
    log_sync_next=5000;
    clock_ms=4999;assert(!sync_logs(0) && !syncs);
    clock_ms=5000;assert(!sync_logs(0) && syncs==1 && log_sync_max_ms==40);
    clock_ms=10039;assert(!sync_logs(0) && syncs==1);
    clock_ms=10040;assert(!sync_logs(0) && syncs==2);
    assert(!sync_logs(1) && syncs==3);
    mock_result=-7;assert(sync_logs(1)==-7 && log_sync_last_result==-7 && log_sync_calls==4);
    log_fd=-1;assert(!sync_logs(1) && syncs==4);
    log_fd=1;mock_result=0;clock_ms=UINT64_C(0x100000000)+5000;
    assert(!sync_logs(0) && syncs==5 && log_sync_next>UINT64_C(0x100000000));
    puts("PASS log sync: worker periodic/forced writeback, no early startup sync, duration measurement, propagated errors, closed log and 64-bit clock");
    return 0;
}
