#include "game_code_audit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE 0x08804000u
static uint8_t memory[4096];
static unsigned failed;
static uint32_t read_mock(uint32_t address,uint8_t* bytes,uint32_t size,void* unused)
{
    (void)unused;
    assert(address>=BASE && address-BASE<sizeof(memory) && size<=sizeof(memory)-(address-BASE));
    if(failed)return failed==2?size-1:0;
    memcpy(bytes,memory+address-BASE,size);return size;
}
static void put(unsigned offset,uint32_t value)
{ unsigned i;for(i=0;i<4;++i)memory[offset+i]=(uint8_t)(value>>(i*8)); }
static void fixture(void)
{
    memset(memory,0,sizeof(memory));failed=0;
    put(0,BASE+64);memory[8]=5;memory[10]=1;
    put(12,BASE+96);put(16,BASE+128);
    memcpy(memory+64,"sceDisplay",11);put(96,0x289D82FE);
    put(256,0x0C000000 | ((BASE+128)>>2));
}
int main(void)
{
    ra_game_map map={1,{{BASE,sizeof(memory)}}};ra_code_audit result;unsigned i;
    ra_address_refs refs;unsigned reg;uint32_t start;ra_table_probe table;
    fixture();
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==0);
    assert(result.imports==1 && result.calls==1 && result.targets[0].nid==0x289D82FE);
    assert(result.sites[0].pc==BASE+256 && result.total_calls==1);
    assert(ra_mips_jump_target(0x0FFFFFFC,0x0C000001)==0x10000004);
    assert(!ra_mips_jump_target(BASE,0x03E00008)); /* JR is not a static J target. */
    memory[8]=0;assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==-2);
    fixture();put(12,0xFFFFFFFF);
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==-2);
    fixture();memory[10]=1;memory[11]=5;
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==-2);
    fixture();put(16,BASE+4092);
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==-2);
    fixture();failed=1;
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==-2);
    fixture();memcpy(memory+64,"UnknownLongLibrary",18);
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==0 && !result.imports);
    fixture();
    assert(ra_game_code_audit(&result,&map,BASE+257,256,BASE,20,read_mock,NULL)==-1);
    assert(ra_game_code_audit(&result,&map,BASE+256,4096,BASE,20,read_mock,NULL)==-1);
    for(i=256;i<512;i+=4)put(i,0x0C000000 | ((BASE+128)>>2));
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==0);
    assert(result.calls==RA_AUDIT_CALLS && result.total_calls==64);
    fixture();put(260,0x08000000 | ((BASE+384)>>2));
    assert(ra_game_find_calls(&result,&map,BASE+256,256,BASE+384,read_mock,NULL)==0);
    assert(result.calls==1 && result.sites[0].pc==BASE+260);
    for(i=256;i<512;i+=4)put(i,0x0C000000 | ((BASE+384)>>2));
    assert(ra_game_find_calls(&result,&map,BASE+256,256,BASE+384,read_mock,NULL)==0);
    assert(result.calls==RA_AUDIT_CALLS && result.total_calls==64);
    assert(ra_game_find_calls(&result,&map,BASE+257,256,BASE+384,read_mock,NULL)==-1);
    assert(ra_game_find_calls(&result,&map,BASE+256,256,BASE+512,read_mock,NULL)==-1);
    assert(ra_game_find_calls(&result,&map,BASE+256,4096,BASE+384,read_mock,NULL)==-1);
    failed=1;assert(ra_game_find_calls(&result,&map,BASE+256,256,BASE+384,read_mock,NULL)==-4);
    fixture();memcpy(memory+64,"InterruptManager",17);put(96,0xCA04A2B9);
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==0 && result.imports==1);
    fixture();memcpy(memory+64,"sceGe_user",11);put(96,0xA4FC06A4);
    assert(ra_game_code_audit(&result,&map,BASE+256,256,BASE,20,read_mock,NULL)==0 && result.imports==1);
    assert(ra_mips_address_pair(0x3C040881,0x2485F180,0x0880F180,&reg) && reg==5);
    assert(ra_mips_address_pair(0x3C040880,0x34854180,BASE+384,&reg));
    assert(!ra_mips_address_pair(0x3C040880,0x24C54180,BASE+384,&reg));
    assert(!ra_mips_address_pair(0x3C000880,0x24054180,BASE+384,&reg));
    assert(!ra_mips_address_pair(0x3C040880,0x24804180,BASE+384,&reg));
    fixture();put(508,0x3C040880);put(512,0x24854180);put(1024,BASE+384);
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==0);
    assert(refs.total_pairs==1 && refs.code[0].upper_pc==BASE+508 && refs.code[0].lower_pc==BASE+512);
    assert(refs.pointers==1 && refs.slots[0]==BASE+1024);
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,0,read_mock,NULL)==0 && !refs.pointers);
    fixture();put(256,0x3C040880);put(284,0x24854180);put(288,0x24854180);
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,0,read_mock,NULL)==0 && refs.total_pairs==1);
    assert(ra_game_address_refs(&refs,&map,BASE+257,512,BASE+384,1,read_mock,NULL)==-1);
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,0xFFFFFFFF,1,read_mock,NULL)==-1);
    for(i=0;i<16;++i){put(256+i*8,0x3C040880);put(260+i*8,0x24854180);put(1024+i*4,BASE+384);}
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==0);
    assert(refs.pairs==RA_AUDIT_REFS && refs.total_pairs>=16 && refs.pointers==RA_AUDIT_REFS && refs.total_pointers==16);
    map.regions[0].address=BASE+1;map.regions[0].size=sizeof(memory)-1;
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==0 && refs.total_pointers==16);
    failed=1;assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==-4);
    failed=2;assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==-4);
    map.regions[0].size=17u*1024*1024;
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==-1);
    map.count=5;assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,1,read_mock,NULL)==-1);
    map.count=1;map.regions[0].address=BASE;map.regions[0].size=sizeof(memory);
    fixture();put(256,0x27BDFFE0);put(260,0xAFBF001C);put(288,0x0C000000|((BASE+384)>>2));
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+288,read_mock,NULL)==1 && start==BASE+256);
    put(260,0x10000001);put(264,0xAFBF001C);
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+288,read_mock,NULL)==0);
    put(260,0x0320F809);
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+288,read_mock,NULL)==0);
    put(264,0);
    put(260,0xAFBF0020);assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+288,read_mock,NULL)==0 && !start);
    put(260,0xAFBF001C);put(280,0x03E00008);
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+288,read_mock,NULL)==0);
    fixture();put(256,0x27BDFFE0);put(292,0xAFBF001C);
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+304,read_mock,NULL)==0);
    put(288,0xAFBF001C);
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+304,read_mock,NULL)==1);
    assert(ra_game_function_start(&start,&map,BASE+256,1024,BASE+768,read_mock,NULL)==1);
    assert(ra_game_function_start(&start,&map,BASE+256,1024,BASE+772,read_mock,NULL)==0);
    assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+289,read_mock,NULL)==-1);
    failed=2;assert(ra_game_function_start(&start,&map,BASE+256,512,BASE+288,read_mock,NULL)==-4);
    assert(ra_mips_address_pair(0x3C040881,0x8C85F180,0x0880F180,&reg));
    assert(ra_mips_address_pair(0x3C040881,0xAC85F180,0x0880F180,&reg));
    fixture();put(256,0x3C040880);put(260,0x8C854180);put(264,0xAC854180);
    assert(ra_game_address_refs(&refs,&map,BASE+256,512,BASE+384,0,read_mock,NULL)==0);
    assert(refs.total_pairs==2 && refs.code[0].operation==35 && refs.code[1].operation==43);
    fixture();put(1024,0);put(1028,BASE+256);put(1032,BASE+2048);put(1036,0xDEADBEEF);
    assert(ra_game_table_probe(&table,&map,BASE+256,512,BASE+1024,4,1,BASE+256,read_mock,NULL)==0);
    assert(table.count==4 && table.kinds[0]==RA_TABLE_ZERO && table.kinds[1]==RA_TABLE_CODE &&
        table.kinds[2]==RA_TABLE_GAME_ADDRESS && table.kinds[3]==RA_TABLE_OPAQUE);
    assert(ra_game_table_probe(&table,&map,BASE+256,512,BASE+1024,4,1,BASE+260,read_mock,NULL)==-2 && !table.count);
    assert(ra_game_table_probe(&table,&map,BASE+256,512,BASE+4092,2,RA_TABLE_WORDS,0,read_mock,NULL)==-1);
    assert(ra_game_table_probe(&table,&map,BASE+256,512,BASE+1024,17,RA_TABLE_WORDS,0,read_mock,NULL)==-1);
    assert(ra_game_table_probe(&table,&map,BASE+256,512,BASE+1024,4,4,0,read_mock,NULL)==-1);
    failed=2;assert(ra_game_table_probe(&table,&map,BASE+256,512,BASE+1024,4,RA_TABLE_WORDS,0,read_mock,NULL)==-4 && !table.count);
    {
        ra_method_audit methods;
        memset(memory,0,sizeof(memory));failed=0;
        /* Object table load at end of one read chunk, call in the next. */
        put(508,0x8E190010);put(512,0x8F390014);put(524,0x0320F809);
        assert(ra_game_method_calls(&methods,&map,BASE+256,512,read_mock,NULL)==0);
        assert(methods.scanned_words==128 && methods.total==1 && methods.chains==1 && methods.count==1);
        assert(methods.sites[0].table_pc==BASE+508 && methods.sites[0].method_pc==BASE+512 && methods.sites[0].call_pc==BASE+524);
        put(524,0x0300F809); /* JALR uses another register. */
        assert(ra_game_method_calls(&methods,&map,BASE+256,512,read_mock,NULL)==0 && !methods.total);
        put(524,0x0320F809);put(512,0x8F390010); /* Other vtable method. */
        assert(ra_game_method_calls(&methods,&map,BASE+256,512,read_mock,NULL)==0 && !methods.total);
        memset(memory,0,sizeof(memory));
        for(i=0;i<40;++i){put(256+i*32,0x8F390014);put(260+i*32,0x0320F809);}
        put(1532,0x8E190010);put(1536,0x8F390014);put(1540,0x0320F809);
        assert(ra_game_method_calls(&methods,&map,BASE+256,1536,read_mock,NULL)==0);
        assert(methods.total==41 && methods.count==32 && methods.chains==1 && methods.scanned_words==384);
        assert(methods.sites[0].table_pc==BASE+1532); /* Chain retained despite earlier generic sites. */
        assert(ra_game_method_calls(&methods,&map,BASE+257,512,read_mock,NULL)==-1);
        assert(ra_game_method_calls(&methods,&map,BASE+256,4096,read_mock,NULL)==-1);
        assert(ra_game_method_calls(&methods,&map,BASE+256,0,read_mock,NULL)==-1);
        failed=2;assert(ra_game_method_calls(&methods,&map,BASE+256,512,read_mock,NULL)==-4);
        failed=1;assert(ra_game_method_calls(&methods,&map,BASE+256,512,read_mock,NULL)==-4);
    }
    puts("PASS code audit: method14 cross-chunk scan/caps/read failures, callers/boundaries, address/LW/SW refs, bounded tables");
    return 0;
}
