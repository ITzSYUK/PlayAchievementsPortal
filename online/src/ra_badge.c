#include "ra_badge.h"
#include <string.h>
static unsigned u16(const unsigned char* p) { return p[0] | (unsigned)p[1]<<8; }
static uint32_t u32(const unsigned char* p)
{ return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
int ra_badge_validate(const void* data,size_t size,const char* disc)
{
    const unsigned char* p=data;
    uint32_t count,hash=2166136261u;
    size_t i,j;
    if(!p || !disc || strlen(disc)!=10 || size<48 || size>RA_BADGE_PACK_LIMIT ||
       ((uintptr_t)p&1) || memcmp(p,"PBAD",4) || u16(p+4)!=1 || u16(p+6)!=48 ||
       memcmp(p+8,disc,10) || p[18] || p[19] || u16(p+20)!=32 || u16(p+22)!=32 ||
       u16(p+24)!=1 || u16(p+26)!=12 || u32(p+44))return 0;
    count=u32(p+28);
    if(!count || count>RA_BADGE_MAX || u32(p+32)!=count*RA_BADGE_BYTES ||
       size!=48u+count*(12u+RA_BADGE_BYTES) || !u32(p+36))return 0;
    for(i=0;i<count;++i) {
        const unsigned char* record=p+48+i*12;
        if(!u32(record) || u32(record+4)!=i*RA_BADGE_BYTES || u32(record+8)!=RA_BADGE_BYTES)return 0;
        for(j=0;j<i;++j)if(u32(p+48+j*12)==u32(record))return 0;
    }
    for(i=0;i<size;++i) { hash^=(i>=40 && i<44)?0:p[i];hash*=16777619u; }
    return hash==u32(p+40);
}
const uint16_t* ra_badge_pixels(const void* data,size_t size,unsigned id)
{
    const unsigned char* p=data;
    uint32_t i,count;
    if(!p || ((uintptr_t)p&1) || size<48 || size>RA_BADGE_PACK_LIMIT)return NULL;
    count=u32(p+28);
    if(count>RA_BADGE_MAX || size!=48u+count*(12u+RA_BADGE_BYTES))return NULL;
    for(i=0;i<count;++i)if(u32(p+48+i*12)==id) {
        unsigned offset=u32(p+52+i*12);
        if(offset>count*RA_BADGE_BYTES || RA_BADGE_BYTES>count*RA_BADGE_BYTES-offset)return NULL;
        return (const uint16_t*)(p+48+count*12+offset);
    }
    return NULL;
}
