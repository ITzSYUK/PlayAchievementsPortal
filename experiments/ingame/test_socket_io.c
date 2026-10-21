#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#define SO_NONBLOCK 0x1009
#define SCE_NET_INET_POLLOUT 4
#define SCE_NET_INET_POLLIN 1
typedef struct { int fd,events,revents; } SceNetInetPollfd;
static unsigned calls,delayed,nonblock=1,returned_size=sizeof(int);
static int fail_call,poll_result;
static int sceNetInetSetsockopt(int fd,int level,int option,const void* value,socklen_t size)
{
    assert(fd==7 && level==SOL_SOCKET);
    if((int)++calls==fail_call)return -99;
    if(option==SO_NONBLOCK)assert(size==sizeof(int) && *(const int*)value==1);
    else {const struct linger* l=value;assert(option==SO_LINGER && size==sizeof(*l) && !l->l_onoff);}
    return 0;
}
static int sceNetInetGetsockopt(int fd,int level,int option,void* value,socklen_t* size)
{
    assert(fd==7 && level==SOL_SOCKET && option==SO_NONBLOCK && *size==sizeof(int));
    if((int)++calls==fail_call)return -99;
    *(int*)value=(int)nonblock;*size=returned_size;return 0;
}
static int sceNetInetSend(int fd,const void* bytes,size_t size,int flags)
{assert(fd==7 && bytes && size==1 && flags==MSG_DONTWAIT);return 1;}
static int sceNetInetRecv(int fd,void* bytes,size_t size,int flags)
{assert(fd==7 && bytes && size==1 && flags==MSG_DONTWAIT);return 1;}
static int sceNetInetPoll(SceNetInetPollfd* fd,size_t count,int timeout)
{assert(fd->fd==7 && count==1 && timeout==0);assert(fd->events==1 || fd->events==4);return poll_result;}
static int sceKernelDelayThread(unsigned us) {delayed=us;return 0;}
#include "../psp/ingame/https/user/socket_io.h"
int main(void)
{
    unsigned i;unsigned char byte=0;
    ra_socket_mode mode;
    assert(!ra_socket_nonblocking(7,&mode) && calls==3 && mode.value==1 && mode.size==sizeof(int));
    for(i=1;i<=3;i+=2){calls=0;fail_call=(int)i;assert(ra_socket_nonblocking(7,&mode)==-99 && calls==i);}
    calls=0;fail_call=2;assert(!ra_socket_nonblocking(7,&mode) && mode.read_result==-99 && calls==3);
    calls=0;fail_call=0;nonblock=0;assert(!ra_socket_nonblocking(7,&mode) && mode.value==0 && calls==3);
    nonblock=128;returned_size=1;assert(!ra_socket_nonblocking(7,&mode) && mode.value==128 && mode.size==1);
    assert(ra_socket_send(7,&byte,1)==1 && ra_socket_recv(7,&byte,1)==1);
    assert(!ra_socket_poll(7,0,999) && delayed==50000);
    delayed=0;assert(!ra_socket_poll(7,1,12) && delayed==12000);
    poll_result=1;delayed=0;assert(ra_socket_poll(7,1,50)==1 && !delayed);
    poll_result=-1;assert(ra_socket_poll(7,0,50)==-1 && !delayed);
    puts("PASS socket I/O: successful setter required, PSP readback variants/unsupported query are diagnostic, no linger, per-call DONTWAIT, zero-time poll, bounded yielding");
    return 0;
}
