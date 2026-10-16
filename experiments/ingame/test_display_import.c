#include "display_import.h"
#include <assert.h>
#include <stdio.h>
static unsigned char bytes[1024];
#define BASE 0x08804000u
static int read_bytes(uint32_t address,void* out,unsigned n,void* unused)
{
    (void)unused;if(address<BASE || address-BASE>sizeof(bytes) || n>sizeof(bytes)-(address-BASE))return 0;
    memcpy(out,bytes+address-BASE,n);return 1;
}
static void put(unsigned pos,uint32_t value)
{ unsigned i;for(i=0;i<4;++i)bytes[pos+i]=(unsigned char)(value>>(i*8)); }
static void prepare(void)
{
    memset(bytes,0,sizeof(bytes));put(0,BASE+100);bytes[8]=5;bytes[10]=1;
    put(12,BASE+200);put(16,BASE+300);memcpy(bytes+100,"sceDisplay",11);
    put(200,0x289d82fe);put(300,0x03e00008);put(304,0x0001230c);
}
int main(void)
{
    uint32_t found=0;
    prepare();assert(ra_display_import(BASE,20,read_bytes,NULL,&found) && found==BASE+300);
    bytes[8]=0;assert(!ra_display_import(BASE,20,read_bytes,NULL,&found));
    prepare();put(12,0xfffffffe);assert(!ra_display_import(BASE,20,read_bytes,NULL,&found));
    prepare();put(16,BASE+1020);assert(!ra_display_import(BASE,20,read_bytes,NULL,&found));
    prepare();put(300,0x08000000);assert(!ra_display_import(BASE,20,read_bytes,NULL,&found));
    prepare();bytes[10]=2;put(204,0x289d82fe);put(308,0x03e00008);put(312,0x0001230c);
    assert(!ra_display_import(BASE,20,read_bytes,NULL,&found));
    prepare();assert(!ra_display_import(BASE,19,read_bytes,NULL,&found));
    assert(!ra_display_import(BASE,65537,read_bytes,NULL,&found));
    puts("PASS display import: unique NID/syscall stub, bounded records/tables/strings, truncated/wrapping/duplicate/foreign hook rejected");
    return 0;
}
