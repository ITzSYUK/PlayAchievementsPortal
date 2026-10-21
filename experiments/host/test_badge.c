#include "ra_badge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char pack[48+2*(12+RA_BADGE_BYTES)] __attribute__((aligned(4)));
static void put(unsigned char* p,unsigned v)
{unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static void checksum(void)
{
    unsigned i,h=2166136261u;memset(pack+40,0,4);
    for(i=0;i<sizeof(pack);++i){h^=pack[i];h*=16777619u;}put(pack+40,h);
}
int main(int argc,char** argv)
{
    unsigned i;const uint16_t* pixels;
    if(argc>1) {
        FILE* f=fopen(argv[1],"rb");size_t n;unsigned char* bytes=malloc(RA_BADGE_PACK_LIMIT+1);
        assert(f && bytes);n=fread(bytes,1,RA_BADGE_PACK_LIMIT+1,f);assert(!ferror(f) && !fclose(f));
        assert(ra_badge_validate(bytes,n,"UCUS-98662") && ra_badge_pixels(bytes,n,1));
        free(bytes);puts("PASS existing PBAD compiler compatibility");return 0;
    }
    memcpy(pack,"PBAD\1\0\60\0UCUS-98662",18);
    pack[20]=pack[22]=32;pack[24]=1;pack[26]=12;
    put(pack+28,2);put(pack+32,2*RA_BADGE_BYTES);put(pack+36,1);
    put(pack+48,1);put(pack+52,0);put(pack+56,RA_BADGE_BYTES);
    put(pack+60,2);put(pack+64,RA_BADGE_BYTES);put(pack+68,RA_BADGE_BYTES);
    for(i=72;i<sizeof(pack);++i)pack[i]=(unsigned char)i;
    checksum();assert(ra_badge_validate(pack,sizeof(pack),"UCUS-98662"));
    pixels=ra_badge_pixels(pack,sizeof(pack),2);assert((const unsigned char*)pixels==pack+72+RA_BADGE_BYTES);
    assert(!ra_badge_pixels(pack,sizeof(pack),3));
    assert(!ra_badge_validate(pack,sizeof(pack),"UCUS-99999"));
    for(i=0;i<sizeof(pack);++i) {
        assert(!ra_badge_validate(pack,i,"UCUS-98662"));
        pack[i]^=1;assert(!ra_badge_validate(pack,sizeof(pack),"UCUS-98662"));pack[i]^=1;
    }
    put(pack+60,1);checksum();assert(!ra_badge_validate(pack,sizeof(pack),"UCUS-98662"));
    put(pack+60,2);put(pack+64,0xffffffffu);checksum();assert(!ra_badge_validate(pack,sizeof(pack),"UCUS-98662"));
    assert(!ra_badge_pixels(pack,sizeof(pack),2));
    puts("PASS PBAD: ID mapping, exact size, game ID, checksum, every truncation/corruption, duplicate/overflow rejection");
    return 0;
}
