#include "frame_candidate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint32_t value;unsigned short_read,bad_signature;} fixture;
static uint32_t read_mock(uint32_t address,uint8_t* bytes,uint32_t size,void* context)
{
    static const uint32_t prefix[]={0x27BDFFF0,0x3C0308AE,0x8C65B21C,0xAFBF000C,
        0x3C0408AE,0x3C030897,0x906319C0,0x24A50001,0x14600008,0xAC85B21C,
        0x0E2572C9,0x24040001};
    fixture* f=context;unsigned i;
    assert((address==RA_CANDIDATE_COUNTER && size==4) || (address==0x8428B0 && size==sizeof(prefix)));
    if(f->short_read)return size-1;
    for(i=0;i<size;++i)bytes[i]=(uint8_t)((address==RA_CANDIDATE_COUNTER?f->value:prefix[i/4])>>((i%4)*8));
    if(address==0x8428B0 && f->bad_signature)bytes[47]^=1;
    return size;
}
int main(void)
{
    fixture f={0};ra_candidate_observation o;
    assert(ra_candidate_signature(read_mock,&f));
    f.bad_signature=1;assert(!ra_candidate_signature(read_mock,&f));f.bad_signature=0;
    f.short_read=1;assert(!ra_candidate_signature(read_mock,&f));f.short_read=0;
    ra_candidate_reset(&o);f.value=UINT32_MAX;
    assert(ra_candidate_poll(&o,0,read_mock,&f));f.value=0;
    assert(ra_candidate_poll(&o,5,read_mock,&f) && o.increments==1);
    assert(ra_candidate_poll(&o,10,read_mock,&f) && o.changes==1);
    f.value=10;assert(ra_candidate_poll(&o,1000,read_mock,&f) && o.gaps==1 && o.increments==1);
    f.value=9;assert(ra_candidate_poll(&o,1005,read_mock,&f) && o.discontinuities==1);
    f.value=12;assert(ra_candidate_poll(&o,1010,read_mock,&f) && o.increments==4 && o.max_delta==3);
    f.short_read=1;assert(!ra_candidate_poll(&o,1015,read_mock,&f) && o.read_errors==1 && !o.have_value);
    f.short_read=0;f.value=100;assert(ra_candidate_poll(&o,1020,read_mock,&f) && o.increments==4);
    assert(ra_candidate_poll(&o,1,read_mock,&f) && o.gaps==2);
    o.increments=UINT32_MAX;f.value=101;
    assert(ra_candidate_poll(&o,6,read_mock,&f) && o.discontinuities==2 && o.increments==UINT32_MAX);
    ra_candidate_reset(&o);assert(!o.have_value && !o.polls && !o.read_errors);
    puts("PASS frame candidate: signature/partial-read rejection, wrap, pause, gaps, reset, overflow, rebaseline");
    return 0;
}
