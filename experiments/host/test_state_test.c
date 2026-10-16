#include "ra_state_test.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned calls,fail,change; uint32_t value; } fixture;
static uint32_t read_bytes(uint32_t address,uint8_t* bytes,uint32_t size,void* context)
{
    fixture* f=context;unsigned i;uint32_t v=f->value;
    assert(address<32 && size<=4);++f->calls;
    if(f->fail)return 0;
    if(f->change && f->calls%2==0)++v;
    for(i=0;i<size;++i)bytes[i]=(uint8_t)(v>>(8*i));
    return size;
}
int main(void)
{
    ra_state_test t={0};fixture f={0};unsigned i;
    const char* rejected[]={"d0xH000001=1","p0xH000001=1","0xH000001=1.2.",
        "R:0xH000001=1","I:0xH000001_0xH000002=1","M:0xH000001=1",
        "A:0xH000001_0xH000002=1","fF000001=1.0","T:0xH000001=1"};
    assert(ra_state_test_prepare(&t,"0xH000001=1"));
    f.value=1;assert(ra_state_test_check(&t,read_bytes,&f)==1 && t.initial_true && !t.matched);
    f.value=0;assert(ra_state_test_check(&t,read_bytes,&f)==0 && t.seen_false);
    f.fail=1;assert(ra_state_test_check(&t,read_bytes,&f)==-1 && !t.matched);f.fail=0;
    f.value=1;f.change=1;f.calls=0;assert(ra_state_test_check(&t,read_bytes,&f)==-2 && !t.matched);f.change=0;
    assert(ra_state_test_check(&t,read_bytes,&f)==1 && t.matched);
    assert(ra_state_test_prepare(&t,"0xH000001=0"));
    f.fail=1;assert(ra_state_test_check(&t,read_bytes,&f)==-1 && !t.seen_false && !t.matched);f.fail=0;
    for(i=0;i<sizeof(rejected)/sizeof(*rejected);++i){
        assert(!ra_state_test_prepare(&t,rejected[i]));
        assert(t.reason==RA_STATE_PARSE || (t.reject_detail && *t.reject_detail));
        assert(ra_state_test_check(&t,read_bytes,&f)==-3);
    }
    assert(!ra_state_test_prepare(&t,"0xH000001=1Sd0xH000002=2"));
    assert(!strcmp(t.reject_detail,"operand_type") && t.reject_group==1 &&
        t.reject_condition==1 && t.reject_operand==1 && t.reject_operand_type==RC_OPERAND_DELTA);
    assert(!ra_state_test_prepare(&t,"0xH000001=d0xH000002"));
    assert(t.reject_operand==2 && t.reject_operand_type==RC_OPERAND_DELTA);
    assert(!ra_state_test_prepare(&t,"0xH000001=1.2."));
    assert(!strcmp(t.reject_detail,"group_flags") && t.reject_hittarget && t.reject_condition==0);
    assert(!ra_state_test_prepare(&t,"T:0xH000001=1"));
    assert(!strcmp(t.reject_detail,"condition_type") && t.reject_type==RC_CONDITION_TRIGGER);
    assert(ra_state_test_prepare(&t,"0xH000001=1_0xH000001!=2"));
    assert(!t.reject_detail);
    f.value=1;f.calls=0;assert(ra_state_test_check(&t,read_bytes,&f)==1 && f.calls==2 && t.refs_count==1);
    assert(ra_state_test_prepare(&t,"0xM000001=1"));assert(ra_state_test_check(&t,read_bytes,&f)==1);
    assert(ra_state_test_prepare(&t,"0xW000001=1"));assert(ra_state_test_check(&t,read_bytes,&f)==1);
    assert(ra_state_test_prepare(&t,"0xH000001=1SP:0xH000002=2_0xH000001=1S0xH000001=3"));
    assert(ra_state_test_check(&t,read_bytes,&f)==1);
    assert(ra_state_test_prepare(&t,"P:0xH000001=1_0xH000002=1"));
    assert(ra_state_test_check(&t,read_bytes,&f)==0 && !t.matched);
    {char large[4098];memset(large,'x',sizeof(large));large[4097]=0;
     assert(!ra_state_test_prepare(&t,large) && t.reason==RA_STATE_LIMIT);}
    assert(!ra_state_test_prepare(&t,NULL));
    ra_state_test_destroy(&t);ra_state_test_destroy(&t);
    puts("PASS manual state: history gate, integer/bit/24bit refs, frozen reads, false-before-true, no zero-on-read-error, cleanup");
    return 0;
}
