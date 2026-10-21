#include "ra_pending.h"
#include <string.h>
static uint32_t u32(const unsigned char* p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put(unsigned char* p,uint32_t v)
{ unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8)); }
int ra_pending_identity(ra_pending* p,const char* account,const char* hash,unsigned game)
{
    unsigned i;
    if(!p || !account || !hash || !game)return 0;
    memset(p,0,sizeof(*p));
    for(i=0;i<65 && account[i];++i) {
        unsigned c=(unsigned char)account[i];
        if(i==64 || c<33 || c>126)return 0;
        p->account[i]=(char)(c>='A' && c<='Z'?c+32:c);
    }
    if(!i)return 0;
    for(i=0;i<32;++i) {
        unsigned c=(unsigned char)hash[i];
        if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return 0;
        p->hash[i]=(char)c;
    }
    if(hash[32])return 0;
    p->game_id=game;p->generation=1;return 1;
}
int ra_pending_add(ra_pending* p,unsigned id,uint64_t earned,unsigned native)
{
    unsigned i;
    if(!id || !earned)return 0;
    for(i=0;i<p->count;++i)if(p->entry[i].id==id)return 1;
    if(p->count==RA_PENDING_MAX || p->generation>=UINT32_MAX-1)return 0;
    p->entry[p->count++]=(ra_pending_entry){id,native,earned,{0}};++p->generation;return 1;
}
void ra_pending_remove(ra_pending* p,unsigned id)
{
    unsigned i;
    for(i=0;i<p->count;++i)if(p->entry[i].id==id) {
        memmove(p->entry+i,p->entry+i+1,(p->count-i-1)*sizeof(*p->entry));
        --p->count;++p->generation;return;
    }
}
size_t ra_pending_encode(const ra_pending* p,void* data,size_t cap,ra_pending_mac mac,void* context)
{
    unsigned char* out=data;size_t size;unsigned i;
    if(!p || p->count>RA_PENDING_MAX || !out || !mac)return 0;
    size=RA_PENDING_HEADER+p->count*RA_PENDING_RECORD;
    if(cap<size+32)return 0;
    memset(out,0,size+32);memcpy(out,"RAPQ",4);put(out+4,1);put(out+8,p->generation);
    put(out+12,p->game_id);put(out+16,p->count);
    memcpy(out+20,p->account,65);memcpy(out+85,p->hash,33);
    for(i=0;i<p->count;++i) {
        unsigned char* e=out+RA_PENDING_HEADER+i*RA_PENDING_RECORD;
        put(e,p->entry[i].id);put(e+4,(uint32_t)p->entry[i].earned);put(e+8,(uint32_t)(p->entry[i].earned>>32));
        memcpy(e+12,p->entry[i].definition,16);
    }
    if(!mac(context,out,size,out+size))return 0;
    return size+32;
}
int ra_pending_decode(ra_pending* out,const void* data,size_t size,const ra_pending* expected,ra_pending_mac mac,void* context)
{
    const unsigned char* p=data;unsigned char signature[32];
    unsigned i,j,count,difference=0;ra_pending identity;
    if(!out || !expected || !p || !mac || size<RA_PENDING_HEADER+32 || size>RA_PENDING_WIRE_MAX)return 0;
    count=u32(p+16);
    if(memcmp(p,"RAPQ",4) || u32(p+4)!=1 || count>RA_PENDING_MAX ||
       size!=RA_PENDING_HEADER+count*RA_PENDING_RECORD+32 || !u32(p+8) || u32(p+8)>=UINT32_MAX-1 ||
       p[84] || p[117] || memcmp(p+118,"\0\0\0\0\0\0\0\0\0\0",10))return 0;
    if(!ra_pending_identity(&identity,(const char*)p+20,(const char*)p+85,u32(p+12)) ||
       strcmp(identity.account,expected->account) || strcmp(identity.hash,expected->hash) ||
       identity.game_id!=expected->game_id)return 0;
    if(!mac(context,p,size-32,signature))return 0;
    for(i=0;i<32;++i)difference|=signature[i]^p[size-32+i];
    memset(signature,0,sizeof(signature));if(difference)return 0;
    identity.generation=u32(p+8);identity.count=count;
    for(i=0;i<count;++i) {
        const unsigned char* e=p+RA_PENDING_HEADER+i*RA_PENDING_RECORD;
        identity.entry[i]=(ra_pending_entry){u32(e),0,(uint64_t)u32(e+4)|(uint64_t)u32(e+8)<<32,{0}};
        memcpy(identity.entry[i].definition,e+12,16);
        if(!identity.entry[i].id || !identity.entry[i].earned)return 0;
        for(j=0;j<i;++j)if(identity.entry[j].id==identity.entry[i].id)return 0;
    }
    *out=identity;return 1;
}
