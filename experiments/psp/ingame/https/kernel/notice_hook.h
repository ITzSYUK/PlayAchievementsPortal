#ifndef RA_NOTICE_HOOK_H
#define RA_NOTICE_HOOK_H
#include "display_import.h"
static uint32_t notice_stub,notice_original[2],notice_jump;
static int notice_read(uint32_t address,void* out,unsigned size,void* unused)
{
    unsigned i;(void)unused;
    if(!control || !ra_game_map_contains(&control->game_map,address,size))return 0;
    for(i=0;i<size;++i)((unsigned char*)out)[i]=((const volatile unsigned char*)(uintptr_t)address)[i];
    return 1;
}
static int install_notice_hook(void)
{
    SceModule* game=sceKernelFindModuleByName("LocoRoco");
    SceKernelModuleInfo info;uint32_t address,wrapper;volatile uint32_t* stub;int intr;
    if(!control || !game || !game->stub_size || !control->notice_wrapper)return -35422;
    memset(&info,0,sizeof(info));info.size=sizeof(info);
    if(sceKernelQueryModuleInfo(child,&info)<0)return -35422;
    wrapper=control->notice_wrapper;
    if(wrapper%4 || info.text_size<4 || wrapper<info.text_addr || wrapper-info.text_addr>info.text_size-4)return -35422;
    if(!ra_display_import((uint32_t)(uintptr_t)game->stub_top,game->stub_size,notice_read,NULL,&address) ||
       ((address+4u)^wrapper)&0xf0000000u)return -35422;
    stub=(volatile uint32_t*)(uintptr_t)address;
    intr=sceKernelCpuSuspendIntr();
    notice_original[0]=stub[0];notice_original[1]=stub[1];
    if(stub[0]!=0x03e00008u || (stub[1]&0xfc00003fu)!=0x0000000cu){sceKernelCpuResumeIntr(intr);return -35422;}
    notice_jump=0x08000000u|((wrapper>>2)&0x03ffffffu);
    stub[1]=0;stub[0]=notice_jump;
    sceKernelDcacheWritebackInvalidateRange((void*)stub,8);
    sceKernelIcacheInvalidateRange((void*)stub,8);
    notice_stub=address;control->notice_installed=1;
    sceKernelCpuResumeIntr(intr);
    ra_probe_log_printf(&logger,"NOTICE_HOOK_INSTALL stub=0x%08X wrapper=0x%08X\n",address,wrapper);
    return 0;
}
static int remove_notice_hook(void)
{
    volatile uint32_t* stub;SceKernelModuleInfo info;int intr;unsigned waits;
    if(!notice_stub)return 0;
    if(game_map_live()<0)return -35422;
    memset(&info,0,sizeof(info));info.size=sizeof(info);
    if(sceKernelQueryModuleInfo(game_uid,&info)<0)return -35422;
    stub=(volatile uint32_t*)(uintptr_t)notice_stub;
    intr=sceKernelCpuSuspendIntr();
    if(stub[0]!=notice_jump || stub[1]!=0){sceKernelCpuResumeIntr(intr);return -35422;}
    stub[1]=notice_original[1];stub[0]=notice_original[0];
    sceKernelDcacheWritebackInvalidateRange((void*)stub,8);
    sceKernelIcacheInvalidateRange((void*)stub,8);
    notice_stub=0;control->notice_installed=0;
    sceKernelCpuResumeIntr(intr);
    for(waits=0;waits<100 && control->notice_active;++waits)sceKernelDelayThread(10000);
    ra_probe_log_printf(&logger,"NOTICE_HOOK_RESTORE active=%u\n",(unsigned)control->notice_active);
    return control->notice_active?-35422:0;
}
#endif
