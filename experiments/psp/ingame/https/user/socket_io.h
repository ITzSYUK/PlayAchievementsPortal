#ifndef RA_SOCKET_IO_H
#define RA_SOCKET_IO_H
/* Transport thread only. No other thread closes or reuses this socket. */
typedef struct { int set_result,read_result,value; unsigned size; } ra_socket_mode;
static int ra_socket_nonblocking(int fd,ra_socket_mode* mode)
{
    int enabled=1, actual=0, rc;
    socklen_t size=sizeof(actual);
    struct linger linger={0,0};
    *mode=(ra_socket_mode){0,-35425,0,0};
    rc=sceNetInetSetsockopt(fd,SOL_SOCKET,SO_NONBLOCK,&enabled,sizeof(enabled));
    mode->set_result=rc;
    if(rc<0)return rc;
    rc=sceNetInetGetsockopt(fd,SOL_SOCKET,SO_NONBLOCK,&actual,&size);
    mode->read_result=rc;mode->value=actual;mode->size=(unsigned)size;
    /* PSP readback semantics are not established: 0.46's exact value/size
     * assertion rejected real hardware. Trust successful setsockopt, retain
     * MSG_DONTWAIT on each BIO call, and report readback without rejecting it. */
    /* Never wait for unsent bytes during close. This does not promise a hard
     * bound on arbitrary CFW/kernel calls; live call diagnostics cover those. */
    return sceNetInetSetsockopt(fd,SOL_SOCKET,SO_LINGER,&linger,sizeof(linger));
}
static int ra_socket_send(int fd,const unsigned char* bytes,size_t size)
{ return sceNetInetSend(fd,bytes,size,MSG_DONTWAIT); }
static int ra_socket_recv(int fd,unsigned char* bytes,size_t size)
{ return sceNetInetRecv(fd,bytes,size,MSG_DONTWAIT); }
static int ra_socket_poll(int fd,int writing,unsigned ms)
{
    SceNetInetPollfd pollfd={fd,writing?SCE_NET_INET_POLLOUT:SCE_NET_INET_POLLIN,0};
    int rc=sceNetInetPoll(&pollfd,1,0);
    /* Poll itself never asks the kernel to wait. Yield only when not ready;
     * caller rechecks cancellation/deadline after at most a 50 ms slice. */
    if(!rc)sceKernelDelayThread((ms>50?50:ms)*1000u);
    return rc;
}
#endif
