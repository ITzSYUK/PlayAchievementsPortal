#include "ra_client_session.h"
#include <mbedtls/sha256.h>
#include <mbedtls/platform_util.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define RA_CLIENT_DIR "mock:/"
#define PSP_O_RDONLY 1
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 4
#define PSP_O_TRUNC 8
typedef int SceUID;
static ra_client_session* runtime;
static ra_pending live,restored;
static unsigned restore_calls,errors,io_calls;
static uint64_t clock_ms=1000;
static int write_failure,sync_failure,short_read,read_open_failure;
static struct { char path[192];unsigned char bytes[RA_PENDING_WIRE_MAX];unsigned size,position,exists; } files[8];
static struct { int unused; } logger;
static uint64_t now_ms(void* p){(void)p;return clock_ms;}
static int64_t utc(void* p){(void)p;return 100000;}
static void ui_network_error(unsigned type,unsigned delay,unsigned reason,int code)
{assert(type==12 && !delay && reason && code);++errors;}
static void ra_probe_log_printf(void* p,const char* format,...){(void)p;assert(format);}
unsigned ra_session_pending_generation(const ra_client_session* s){(void)s;return live.generation;}
int ra_session_pending_snapshot(ra_client_session* s,ra_pending* out){(void)s;*out=live;return 1;}
int ra_session_pending_restore(ra_client_session* s,const ra_pending* source,uint64_t at)
{(void)s;assert(at==100000);restored=*source;++restore_calls;return 1;}
static int sceIoOpen(const char* path,int flags,int mode)
{
    unsigned i;(void)mode;++io_calls;
    if(flags==PSP_O_RDONLY && read_open_failure)return -9;
    for(i=0;i<8;++i)if(files[i].exists && !strcmp(files[i].path,path))break;
    if(i==8 && flags&PSP_O_CREAT) {
        for(i=0;i<8 && files[i].exists;++i){}
        assert(i<8);strcpy(files[i].path,path);files[i].exists=1;
    }
    if(i==8)return (int)UINT32_C(0x80010002);
    if(flags&PSP_O_TRUNC)files[i].size=0;
    files[i].position=0;return (int)i;
}
static int sceIoRead(int fd,void* bytes,unsigned size)
{
    unsigned n;++io_calls;assert(fd>=0 && fd<8);
    n=files[fd].size-files[fd].position;if(n>size)n=size;
    if(short_read && n>7)n=7;
    memcpy(bytes,files[fd].bytes+files[fd].position,n);files[fd].position+=n;return (int)n;
}
static int sceIoWrite(int fd,const void* bytes,unsigned size)
{
    unsigned n=size>17?17:size;++io_calls;assert(fd>=0 && fd<8);
    if(write_failure && files[fd].position>=17)return -1;
    assert(n<=sizeof(files[fd].bytes)-files[fd].position);
    memcpy(files[fd].bytes+files[fd].position,bytes,n);files[fd].position+=n;
    files[fd].size=files[fd].position;return (int)n;
}
static int sceIoClose(int fd){assert(fd>=0 && fd<8);++io_calls;return 0;}
static int sceIoSync(const char* path,int flags){assert(!strcmp(path,"ms0:") && !flags);++io_calls;return sync_failure?-1:0;}
#include "../psp/ingame/https/user/pending_store.h"
static void restart(void)
{
    pending_enabled=1;pending_initialized=pending_blocked=pending_slot=pending_written=0;
    pending_gate=pending_ready=pending_seen=0;pending_retry_at=0;
    memset(&pending_dirty,0,sizeof(pending_dirty));memset(pending_path,0,sizeof(pending_path));
}
static void wire_boundaries(void)
{
    unsigned char wire[RA_PENDING_WIRE_MAX],saved[RA_PENDING_WIRE_MAX];
    ra_pending decoded,other;size_t n,i;
    assert(ra_pending_identity(&live,"Owner","0123456789abcdef0123456789abcdef",1));
    assert(ra_pending_add(&live,1,99900,1));
    n=ra_pending_encode(&live,wire,sizeof(wire),pending_mac,NULL);assert(n);
    memcpy(saved,wire,n);
    for(i=0;i+sizeof(pending_key)<=n;++i)assert(memcmp(wire+i,pending_key,sizeof(pending_key)));
    assert(ra_pending_decode(&decoded,wire,n,&live,pending_mac,NULL));
    assert(!strcmp(decoded.account,"owner") && decoded.count==1 && !decoded.entry[0].native);
    for(i=0;i<n;++i) {
        assert(!ra_pending_decode(&decoded,wire,i,&live,pending_mac,NULL));
        wire[i]^=1;assert(!ra_pending_decode(&decoded,wire,n,&live,pending_mac,NULL));wire[i]^=1;
    }
    assert(!memcmp(saved,wire,n));
    assert(ra_pending_identity(&other,"Other","0123456789abcdef0123456789abcdef",1));
    assert(!ra_pending_decode(&decoded,wire,n,&other,pending_mac,NULL));
    other=live;other.hash[0]='f';assert(!ra_pending_decode(&decoded,wire,n,&other,pending_mac,NULL));
    other=live;other.game_id=2;assert(!ra_pending_decode(&decoded,wire,n,&other,pending_mac,NULL));
    pending_key[0]^=1;assert(!ra_pending_decode(&decoded,wire,n,&live,pending_mac,NULL));pending_key[0]^=1;
}
int main(void)
{
    unsigned char signature[32];char hex[65];unsigned i,before;
    /* RFC 4231 case 2. Production HMAC wrapper, not a fake checksum. */
    memcpy(pending_key,"Jefe",4);
    assert(pending_mac(NULL,"what do ya want for nothing?",28,signature));
    for(i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",signature[i]);
    assert(!strcmp(hex,"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    wire_boundaries();
    restart();assert(pending_load() && !restore_calls && !pending_blocked);
    before=io_calls;pending_capture();assert(io_calls==before);pending_service();
    assert(pending_written==live.generation && !errors);
    assert(ra_pending_add(&live,2,99901,1));write_failure=1;pending_capture();pending_service();
    assert(errors==1 && pending_written<live.generation);write_failure=0;
    restart();short_read=1;assert(pending_load() && restore_calls==1 && restored.count==1 && !pending_blocked);
    short_read=0;
    /* An invalid sibling never hides a complete authenticated generation. */
    pending_capture();pending_service();assert(pending_written==live.generation);
    assert(ra_pending_add(&live,3,99902,1));sync_failure=1;pending_capture();pending_service();
    assert(errors==2);sync_failure=0;
    /* New account has another path and cannot see this account's queue. */
    assert(ra_pending_identity(&live,"Other","0123456789abcdef0123456789abcdef",1));
    restart();assert(pending_load() && restore_calls==1 && !pending_blocked);
    read_open_failure=1;restart();assert(pending_load() && pending_blocked && restore_calls==1);
    before=io_calls;pending_capture();pending_service();assert(io_calls==before);
    read_open_failure=0;
    assert(ra_pending_identity(&live,"Owner","0123456789abcdef0123456789abcdef",1));
    pending_key[0]^=1;restart();assert(pending_load() && pending_blocked && restore_calls==1);
    before=io_calls;pending_capture();pending_service();assert(io_calls==before);
    puts("PASS pending storage: RFC HMAC-SHA256, every truncation/byte corruption, account/hash/set/key isolation, partial reads/writes, torn sibling, sync failure, invalid files retained, no owner I/O");
    return 0;
}
