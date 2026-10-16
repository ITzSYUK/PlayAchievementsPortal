#ifndef RA_DISPLAY_IMPORT_H
#define RA_DISPLAY_IMPORT_H
#include <stdint.h>
#include <string.h>
typedef int (*ra_import_read)(uint32_t,void*,unsigned,void*);
static inline uint32_t ra_import_u32(const unsigned char* p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
/* PSP little-endian import records. Never dereference unvalidated pointers.
 * Return one unambiguous sceDisplaySetFrameBuf function stub, otherwise fail. */
static inline int ra_display_import(uint32_t top,unsigned bytes,ra_import_read read,void* ctx,uint32_t* found)
{
    unsigned offset=0,libs=0,matches=0;uint32_t result=0;
    if(!bytes || bytes>65536u || (uint64_t)top+bytes>UINT32_MAX)return 0;
    while(offset<bytes && ++libs<=128) {
        unsigned char h[20];unsigned len,count,i;char name[11];uint32_t nids,stubs;
        if(bytes-offset<20 || !read(top+offset,h,20,ctx))return 0;
        len=(unsigned)h[8]*4;count=(unsigned)h[10]|((unsigned)h[11]<<8);
        if(len<20 || len>64 || len>bytes-offset || count>2048)return 0;
        if(!read(ra_import_u32(h),name,sizeof(name),ctx))return 0;
        if(!memcmp(name,"sceDisplay",11)) {
            nids=ra_import_u32(h+12);stubs=ra_import_u32(h+16);
            if((uint64_t)nids+count*4u>UINT32_MAX || (uint64_t)stubs+count*8u>UINT32_MAX)return 0;
            for(i=0;i<count;++i) {
                unsigned char value[8];
                if(!read(nids+i*4u,value,4,ctx))return 0;
                if(ra_import_u32(value)!=0x289d82feu)continue;
                if(!read(stubs+i*8u,value,8,ctx))return 0;
                if(ra_import_u32(value)!=0x03e00008u ||
                   (ra_import_u32(value+4)&0xfc00003fu)!=0x0000000cu)return 0;
                result=stubs+i*8u;++matches;
            }
        }
        offset+=len;
    }
    if(offset!=bytes || matches!=1)return 0;
    *found=result;return 1;
}
#endif
