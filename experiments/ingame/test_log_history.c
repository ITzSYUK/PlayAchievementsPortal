#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RA_CLIENT_DIR "ms0:/seplugins/RAClientLite/"
#define PSP_O_RDONLY 1
static struct { char path[96], text[256]; } files[8];
static int read_error, close_error, rename_error, remove_error, open_error;
static unsigned mutations;
static int find(const char* path)
{
    unsigned i;
    for (i=0;i<8;++i) if (!strcmp(files[i].path,path)) return (int)i;
    return -1;
}
static int missing(void) { return (int)UINT32_C(0x80010002); }
static int sceIoOpen(const char* path, int flags, int mode)
{ int i=find(path); assert(flags==PSP_O_RDONLY && mode==0); return open_error ? open_error : (i<0 ? missing() : i); }
static int sceIoRead(int fd, void* bytes, unsigned size)
{
    unsigned length=(unsigned)strlen(files[fd].text);
    if (read_error) return read_error;
    if (length>size) length=size;
    memcpy(bytes,files[fd].text,length); return (int)length;
}
static int sceIoClose(int fd) { (void)fd; return close_error; }
static int sceIoRemove(const char* path)
{
    int i=find(path);
    if (remove_error) return remove_error;
    if(i<0) return missing();
    files[i].path[0]=0; ++mutations; return 0;
}
static int sceIoRename(const char* source,const char* target)
{
    int i=find(source);
    if(rename_error) return rename_error;
    if(i<0) return missing();
    assert(find(target)<0); strcpy(files[i].path,target); ++mutations; return 0;
}
#include "log_history.h"
static void put(const char* component,unsigned age,const char* text)
{
    char path[96]; unsigned i;
    ra_log_path(path,component,age);
    assert(find(path)<0);
    for(i=0;i<8;++i) if(!files[i].path[0]) {
        strcpy(files[i].path,path);strcpy(files[i].text,text);return;
    }
    assert(0);
}
static void expect(const char* component,unsigned age,const char* text)
{
    char path[96];int i;
    ra_log_path(path,component,age);i=find(path);
    if (!text) assert(i<0); else assert(i>=0 && !strcmp(files[i].text,text));
}
static void reset(void)
{ memset(files,0,sizeof(files));read_error=close_error=rename_error=remove_error=open_error=0;mutations=0; }
int main(void)
{
    uint32_t id=0;
    unsigned launch,age;
    char text[64];
    reset();
    put("bootstrap",0,"client_version=0.41 legacy\n");put("user",0,"legacy user\n");
    assert(ra_log_begin_history(&id)==0 && id==1);
    expect("bootstrap",1,"client_version=0.41 legacy\n");expect("user",1,"legacy user\n");
    /* Hardware acceptance scenario: the fifth launch evicts session 1.
     * Seed the legacy pair too, as on the owner's Memory Stick. */
    for(launch=1;launch<=5;++launch) {
        if(launch>1) assert(ra_log_begin_history(&id)==0 && id==launch);
        snprintf(text,sizeof(text),"LOG_SESSION id=%u component=bootstrap\n",id);put("bootstrap",0,text);
        snprintf(text,sizeof(text),"LOG_SESSION id=%u component=user\n",id);put("user",0,text);
    }
    for(age=0;age<=3;++age) {
        snprintf(text,sizeof(text),"LOG_SESSION id=%u component=bootstrap\n",5-age);
        expect("bootstrap",age,text);
        snprintf(text,sizeof(text),"LOG_SESSION id=%u component=user\n",5-age);
        expect("user",age,text);
    }
    for(age=0;age<8;++age) {
        assert(files[age].path[0]);
        assert(!strstr(files[age].text,"id=1 "));
        assert(!strstr(files[age].text,"legacy"));
    }
    reset();
    for(launch=1;launch<=12;++launch) {
        assert(ra_log_begin_history(&id)==0 && id==launch);
        for(age=1;age<=3 && age<launch;++age) {
            snprintf(text,sizeof(text),"LOG_SESSION id=%u component=bootstrap\n",launch-age);
            expect("bootstrap",age,text);
            /* Launch 7 died before the user log could be created. */
            if(launch-age==7) expect("user",age,NULL);
            else {snprintf(text,sizeof(text),"user session %u\n",launch-age);expect("user",age,text);}
        }
        snprintf(text,sizeof(text),"LOG_SESSION id=%u component=bootstrap\n",id);put("bootstrap",0,text);
        if(launch!=7){snprintf(text,sizeof(text),"user session %u\n",id);put("user",0,text);}
    }
    /* Crash after rotation: no current bootstrap. Recover from newest archive. */
    assert(ra_log_rotate("bootstrap")==0);
    assert(ra_log_begin_history(&id)==0 && id==13);
    reset();put("bootstrap",0,"LOG_SESSION id=4294967295 component=bootstrap\n");
    assert(ra_log_begin_history(&id)==RA_LOG_HISTORY_ERROR && mutations==0);
    reset();put("bootstrap",0,"LOG_SESSION id=4294967296 component=bootstrap\n");
    assert(ra_log_begin_history(&id)==RA_LOG_HISTORY_ERROR && mutations==0);
    reset();put("bootstrap",0,"LOG_SESSION id=12");
    assert(ra_log_begin_history(&id)==RA_LOG_HISTORY_ERROR && mutations==0);
    reset();put("bootstrap",0,"LOG_SESSION id=");
    assert(ra_log_begin_history(&id)==RA_LOG_HISTORY_ERROR && mutations==0);
    reset();put("bootstrap",0,"LOG_SESSION id=4 component=bootstrap\n");
    read_error=-99;assert(ra_log_begin_history(&id)==-99 && mutations==0);
    read_error=0;close_error=-98;assert(ra_log_begin_history(&id)==-98 && mutations==0);
    close_error=0;open_error=-97;assert(ra_log_begin_history(&id)==-97 && mutations==0);
    open_error=0;remove_error=-96;assert(ra_log_begin_history(&id)==-96 && mutations==0);
    remove_error=0;rename_error=-95;assert(ra_log_begin_history(&id)==-95 && mutations==0);
    expect("bootstrap",0,"LOG_SESSION id=4 component=bootstrap\n");
    puts("PASS log history: launch 5 retains exactly pairs 5/4/3/2 (8 files), 12 launches, legacy/crash/missing user, counter overflow, read/open/close/remove/rename failures");
    return 0;
}
