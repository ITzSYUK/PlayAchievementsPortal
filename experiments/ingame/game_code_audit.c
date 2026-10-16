#include "game_code_audit.h"
#include <string.h>
static uint32_t u32(const uint8_t* p)
{ return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
uint32_t ra_mips_jump_target(uint32_t pc, uint32_t instruction)
{
    unsigned op=instruction>>26;
    return op==2 || op==3 ? ((pc+4)&UINT32_C(0xF0000000)) | ((instruction&UINT32_C(0x03FFFFFF))<<2) : 0;
}
static int read_bytes(const ra_game_map* map, ra_psp_platform_read_t read, void* context,
    uint32_t address, uint8_t* bytes, uint32_t size)
{ return ra_game_map_contains(map,address,size) && read(address,bytes,size,context)==size; }
int ra_game_method_calls(ra_method_audit* out,const ra_game_map* map,uint32_t text,uint32_t text_size,
    ra_psp_platform_read_t read,void* context)
{
    uint8_t bytes[256];uint32_t history[16]={0},offset;unsigned seen=0;
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!read || text%4 || !text_size || text_size%4 || text_size>4u*1024*1024 ||
        !ra_game_map_contains(map,text,text_size))return -1;
    for(offset=0;offset<text_size;){
        uint32_t amount=text_size-offset,i;
        if(amount>sizeof(bytes))amount=sizeof(bytes);
        if(!read_bytes(map,read,context,text+offset,bytes,amount))return -4;
        for(i=0;i<amount;i+=4){
            uint32_t word=u32(bytes+i),pc=text+offset+i;unsigned j;
            if(word>>26==0 && (word&63u)==9 && ((word>>21)&31u)){
                unsigned called=(word>>21)&31u;
                for(j=1;j<=8 && j<=seen;++j){
                    uint32_t load=history[(seen-j)%16];unsigned base=(load>>21)&31u,k;
                    uint32_t table_pc=0;
                    if(load>>26!=35 || (load&65535u)!=20 || ((load>>16)&31u)!=called || !base)continue;
                    for(k=j+1;k<=j+8 && k<=seen;++k){
                        uint32_t table=history[(seen-k)%16];
                        if(table>>26==35 && (table&65535u)==16 && ((table>>16)&31u)==base && ((table>>21)&31u)){
                            table_pc=pc-k*4;break;
                        }
                    }
                    ++out->total;if(table_pc)++out->chains;
                    {unsigned slot=out->count;
                    if(slot==RA_METHOD_SITES && table_pc){
                        for(slot=0;slot<out->count;++slot)if(!out->sites[slot].table_pc)break;
                    }
                    if(slot<RA_METHOD_SITES){
                        ra_method_site* site=&out->sites[slot];
                        if(out->count<RA_METHOD_SITES)++out->count;
                        site->table_pc=table_pc;site->method_pc=pc-j*4;site->call_pc=pc;
                    }}
                    break; /* Nearest matching load: one candidate per call. */
                }
            }
            history[seen%16]=word;++seen;++out->scanned_words;
        }
        offset+=amount;
    }
    return 0;
}
int ra_game_find_calls(ra_code_audit* out,const ra_game_map* map,uint32_t text,uint32_t text_size,
    uint32_t target_address,ra_psp_platform_read_t read,void* context)
{
    uint8_t bytes[256];uint32_t offset;
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!read || text%4 || !text_size || text_size%4 || text_size>4u*1024*1024 ||
        !ra_game_map_contains(map,text,text_size) || target_address%4 ||
        target_address<text || target_address-text>=text_size)return -1;
    for(offset=0;offset<text_size;){
        uint32_t amount=text_size-offset,i;
        if(amount>sizeof(bytes))amount=sizeof(bytes);
        if(!read_bytes(map,read,context,text+offset,bytes,amount))return -4;
        for(i=0;i<amount;i+=4){
            uint32_t pc=text+offset+i,instruction=u32(bytes+i);
            if(ra_mips_jump_target(pc,instruction)==target_address){
                ++out->total_calls;
                if(out->calls<RA_AUDIT_CALLS){
                    ra_audit_call* call=&out->sites[out->calls++];
                    call->pc=pc;call->instruction=instruction;
                }
            }
        }
        offset+=amount;
    }
    return 0;
}
int ra_mips_address_pair(uint32_t upper,uint32_t lower,uint32_t target_address,unsigned* reg)
{
    unsigned base=(upper>>16)&31u,dest=(lower>>16)&31u,op=lower>>26;
    uint32_t value=(upper&65535u)<<16,low=lower&65535u;
    if(upper>>26!=15 || ((upper>>21)&31u) || !base || (!dest && op!=43) ||
        ((lower>>21)&31u)!=base || (op!=9 && op!=13 && op!=35 && op!=43))return 0;
    if(op!=13)value+=(low&0x8000u)?low|0xFFFF0000u:low;
    else value|=low;
    if(value!=target_address)return 0;
    if(reg)*reg=dest;
    return 1;
}
int ra_game_address_refs(ra_address_refs* out,const ra_game_map* map,uint32_t text,uint32_t text_size,
    uint32_t target_address,int scan_pointers,ra_psp_platform_read_t read,void* context)
{
    uint8_t bytes[288];uint32_t offset,total_bytes=0;unsigned segment;
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!read || text%4 || !text_size || text_size%4 || text_size>4u*1024*1024 ||
        !ra_game_map_contains(map,text,text_size) || !ra_game_map_contains(map,target_address,4))return -1;
    for(segment=0;segment<map->count;++segment){
        if(map->regions[segment].size>16u*1024*1024-total_bytes)return -1;
        total_bytes+=map->regions[segment].size;
    }
    for(offset=0;offset<text_size;){
        uint32_t amount=text_size-offset,loaded,i;
        if(amount>256)amount=256;
        loaded=text_size-offset;
        if(loaded>amount+28)loaded=amount+28;
        if(!read_bytes(map,read,context,text+offset,bytes,loaded))return -4;
        for(i=0;i<amount;i+=4){
            uint32_t j,upper=u32(bytes+i);
            if(upper>>26!=15)continue;
            for(j=i+4;j<=i+28 && j+4<=loaded;j+=4){
                unsigned reg;
                if(ra_mips_address_pair(upper,u32(bytes+j),target_address,&reg)){
                    ++out->total_pairs;
                    if(out->pairs<RA_AUDIT_REFS){
                        ra_address_pair* pair=&out->code[out->pairs++];
                        pair->upper_pc=text+offset+i;pair->lower_pc=text+offset+j;pair->reg=reg;
                        pair->operation=u32(bytes+j)>>26;
                    }
                }
            }
        }
        offset+=amount;
    }
    if(!scan_pointers)return 0;
    for(segment=0;segment<map->count;++segment){
        uint32_t first=map->regions[segment].address,size=map->regions[segment].size;
        uint32_t skip=(4-first%4)%4;
        if(skip>=size)continue;
        first+=skip;size-=skip;size-=size%4;
        for(offset=0;offset<size;){
            uint32_t amount=size-offset,i;
            if(amount>256)amount=256;
            if(!read_bytes(map,read,context,first+offset,bytes,amount))return -4;
            for(i=0;i<amount;i+=4)if(u32(bytes+i)==target_address){
                ++out->total_pointers;
                if(out->pointers<RA_AUDIT_REFS)out->slots[out->pointers++]=first+offset+i;
            }
            offset+=amount;
        }
    }
    return 0;
}
int ra_game_table_probe(ra_table_probe* out,const ra_game_map* map,uint32_t text,uint32_t text_size,
    uint32_t table,unsigned words,unsigned guard_index,uint32_t expected,ra_psp_platform_read_t read,void* context)
{
    uint8_t bytes[RA_TABLE_WORDS*4];unsigned i;
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!read || text%4 || !text_size || text_size%4 || text_size>4u*1024*1024 || table%4 ||
        !words || words>RA_TABLE_WORDS || (guard_index!=RA_TABLE_WORDS && guard_index>=words) ||
        !ra_game_map_contains(map,text,text_size) || !ra_game_map_contains(map,table,words*4))return -1;
    if(!read_bytes(map,read,context,table,bytes,words*4))return -4;
    if(guard_index!=RA_TABLE_WORDS && u32(bytes+guard_index*4)!=expected)return -2;
    for(i=0;i<words;++i){
        uint32_t value=u32(bytes+i*4);out->values[i]=value;
        if(!value)out->kinds[i]=RA_TABLE_ZERO;
        else if(value%4==0 && value>=text && value-text<text_size)out->kinds[i]=RA_TABLE_CODE;
        else if(ra_game_map_contains(map,value,4))out->kinds[i]=RA_TABLE_GAME_ADDRESS;
        else out->kinds[i]=RA_TABLE_OPAQUE;
    }
    out->count=words;return 0;
}
int ra_game_function_start(uint32_t* start,const ra_game_map* map,uint32_t text,uint32_t text_size,
    uint32_t call_pc,ra_psp_platform_read_t read,void* context)
{
    uint8_t bytes[516];uint32_t first,offset,size;
    if(!start)return -1;
    *start=0;
    if(!read || text%4 || !text_size || text_size%4 || text_size>4u*1024*1024 || call_pc%4 ||
        !ra_game_map_contains(map,text,text_size) || call_pc<text || call_pc-text>=text_size)return -1;
    first=call_pc-text>=512?call_pc-512:text;
    size=call_pc-first+4;
    if(!read_bytes(map,read,context,first,bytes,size))return -4;
    offset=size-4;
    for(;;){
        uint32_t instruction=u32(bytes+offset);
        if(instruction==0x03E00008)return 0;
        if((instruction&0xFFFF0000u)==0x27BD0000u && (instruction&0x8000u)){
            uint32_t frame=0x10000u-(instruction&65535u),j;
            if(frame>=16 && frame<=4096 && frame%8==0){
                for(j=offset+4;j<=offset+32 && j+4<=size;j+=4){
                    uint32_t save=u32(bytes+j),slot=save&65535u;
                    unsigned op=save>>26,fn=save&63u;
                    if((save&0xFFFF0000u)==0xAFBF0000u && slot%4==0 && slot<=frame-4){
                        *start=first+offset;return 1;
                    }
                    /* A boundary with no verified RA save is not promoted
                     * across another stack allocation or control transfer. */
                    if((op>=1 && op<=7) || (op>=20 && op<=23) ||
                        (op==0 && (fn==8 || fn==9)) ||
                        (op>=16 && op<=18 && ((save>>21)&31u)==8) ||
                        (save&0xFFFF0000u)==0x27BD0000u)break;
                }
            }
            return 0; /* Do not skip an ambiguous allocation to an older function. */
        }
        if(!offset)break;
        offset-=4;
    }
    return 0;
}
static int library(const ra_game_map* map, ra_psp_platform_read_t read, void* context, uint32_t address)
{
    char name[24]; unsigned i;
    if(!address)return 0;
    for(i=0;i<sizeof(name);++i){
        if(address>UINT32_MAX-i || !read_bytes(map,read,context,address+i,(uint8_t*)&name[i],1)) return -1;
        if(!name[i]){
            if(!strcmp(name,"sceDisplay"))return 1;
            if(!strcmp(name,"sceCtrl"))return 2;
            if(!strcmp(name,"sceGe_user"))return 3;
            if(!strcmp(name,"InterruptManager"))return 4;
            return 0;
        }
    }
    return 0;
}
static int target(int lib,uint32_t nid)
{
    return (lib==1 && (nid==0x289D82FE || nid==0x984C27E7 || nid==0x46F186C3)) ||
           (lib==2 && (nid==0x3A622550 || nid==0x1F803938)) ||
           (lib==3 && (nid==0x03444EB4 || nid==0xB287BD61 || nid==0xA4FC06A4 || nid==0x05DB22CE)) ||
           (lib==4 && (nid==0xCA04A2B9 || nid==0xD61E6961 || nid==0xFB8E22EC || nid==0x8A389411));
}
int ra_game_code_audit(ra_code_audit* out,const ra_game_map* map,uint32_t text,uint32_t text_size,
    uint32_t stubs,uint32_t stub_size,ra_psp_platform_read_t read,void* context)
{
    uint32_t cursor=0,offset,functions=0;
    uint8_t bytes[256]; unsigned i;
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!read || text%4 || !text_size || text_size%4 || text_size>4u*1024*1024 ||
        !ra_game_map_contains(map,text,text_size) || !stub_size || stub_size>16384 || stubs%4 ||
        !ra_game_map_contains(map,stubs,stub_size))return -1;
    while(cursor<stub_size){
        uint32_t length,count,nids,table; int lib;
        if(++out->libraries>128 || stub_size-cursor<20 ||
            !read_bytes(map,read,context,stubs+cursor,bytes,20))return -2;
        length=(uint32_t)bytes[8]*4;count=bytes[10]|(uint32_t)bytes[11]<<8;
        nids=u32(bytes+12);table=u32(bytes+16);
        if(length<20 || length>stub_size-cursor || count>1024 || count>4096-functions)return -2;
        functions+=count;
        lib=library(map,read,context,u32(bytes));if(lib<0)return -2;
        if(lib && count){
            if(nids%4 || table%4 || !ra_game_map_contains(map,nids,count*4) ||
                !ra_game_map_contains(map,table,count*8))return -2;
            for(i=0;i<count;++i){
                uint32_t nid;
                if(!read_bytes(map,read,context,nids+i*4,bytes,4))return -2;
                nid=u32(bytes);
                if(target(lib,nid)){
                    if(out->imports==RA_AUDIT_IMPORTS)return -3;
                    out->targets[out->imports].nid=nid;out->targets[out->imports++].stub=table+i*8;
                }
            }
        }
        cursor+=length;
    }
    for(offset=0;offset<text_size;){
        uint32_t amount=text_size-offset;
        if(amount>sizeof(bytes))amount=sizeof(bytes);
        if(!read_bytes(map,read,context,text+offset,bytes,amount))return -4;
        for(i=0;i<amount;i+=4){
            uint32_t instruction=u32(bytes+i),pc=text+offset+i,t=ra_mips_jump_target(pc,instruction);
            unsigned j;
            for(j=0;t && j<out->imports;++j)if(t==out->targets[j].stub){
                ++out->total_calls;
                if(out->calls<RA_AUDIT_CALLS){
                    ra_audit_call* call=&out->sites[out->calls++];
                    call->pc=pc;call->instruction=instruction;call->import_index=j;
                }
                break;
            }
        }
        offset+=amount;
    }
    return 0;
}
