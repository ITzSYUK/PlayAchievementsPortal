#include "ra_state_test.h"
#include <stdlib.h>
#include <string.h>
void ra_state_test_destroy(ra_state_test* t)
{
    if(!t)return;
    free(t->buffer);memset(t,0,sizeof(*t));
}
const char* ra_state_reason(unsigned reason)
{
    static const char* const names[]={"not_found","ready","limit","parse","history_dependent","unsupported_shape"};
    return reason<sizeof(names)/sizeof(*names)?names[reason]:"unknown";
}
static int operand(ra_state_test* t,const rc_operand_t* op)
{
    unsigned i;uint32_t size;
    if(op->type==RC_OPERAND_CONST)return 1;
    t->reject_operand_type=op->type;t->reject_access=op->memref_access_type;t->reject_size=op->size;
    if(op->type!=RC_OPERAND_ADDRESS){t->reject_detail="operand_type";return 0;}
    if(op->memref_access_type!=RC_OPERAND_ADDRESS){t->reject_detail="operand_access";return 0;}
    if(op->is_combining){t->reject_detail="operand_combining";return 0;}
    if(!op->value.memref){t->reject_detail="missing_memref";return 0;}
    if(op->size>RC_MEMSIZE_BITCOUNT){t->reject_detail="operand_size";return 0;}
    if(op->value.memref->value.memref_type!=0){t->reject_detail="memref_type";return 0;}
    if(op->value.memref->value.size>RC_MEMSIZE_32_BITS){t->reject_detail="memref_size";return 0;}
    /* v12.5.0 canonical integer memref sizes 0..3 mean 1..4 bytes.
     * memref_type 0 is a direct rc_memref_t, never a modified/indirect ref. */
    size=op->value.memref->value.size+1;
    for(i=0;i<t->refs_count;++i)
        if(t->refs[i].address==op->value.memref->address && t->refs[i].size==size)return 1;
    if(t->refs_count==RA_STATE_TEST_REFS){t->reject_detail="refs_limit";return 0;}
    t->refs[t->refs_count].address=op->value.memref->address;
    t->refs[t->refs_count++].size=size;return 1;
}
static int group(ra_state_test* t,const rc_condset_t* set,unsigned* count)
{
    const rc_condition_t* c;
    if(!set)return 1;
    t->reject_condition=0;t->reject_operand=0;
    t->reject_type=0;t->reject_operator=0;t->reject_hits=0;
    t->reject_operand_type=0;t->reject_access=0;t->reject_size=0;
    t->reject_indirect=set->num_indirect_conditions;t->reject_hittarget=set->num_hittarget_conditions;
    t->reject_reset=set->num_reset_conditions;t->reject_measured=set->num_measured_conditions;
    if(set->num_indirect_conditions || set->num_hittarget_conditions || set->num_reset_conditions || set->num_measured_conditions){t->reject_detail="group_flags";return 0;}
    for(c=set->conditions;c;c=c->next){
        ++t->reject_condition;t->reject_operand=0;
        t->reject_type=c->type;t->reject_operator=c->oper;t->reject_hits=c->required_hits;
        t->reject_operand_type=0;t->reject_access=0;t->reject_size=0;
        if(++*count>128){t->reject_detail="conditions_limit";return 0;}
        if(c->required_hits){t->reject_detail="required_hits";return 0;}
        if(c->type!=RC_CONDITION_STANDARD && c->type!=RC_CONDITION_PAUSE_IF){t->reject_detail="condition_type";return 0;}
        if(c->oper>RC_OPERATOR_NE){t->reject_detail="operator";return 0;}
        t->reject_operand=1;if(!operand(t,&c->operand1))return 0;
        t->reject_operand=2;if(!operand(t,&c->operand2))return 0;
    }
    return 1;
}
int ra_state_test_prepare(ra_state_test* t,const char* definition)
{
    size_t length=0;int size;unsigned groups=0,count=0;
    const rc_condset_t* set;
    if(!t)return 0;
    ra_state_test_destroy(t);
    if(!definition){t->reason=RA_STATE_PARSE;return 0;}
    while(length<=4096 && definition[length])++length;
    if(!length || length>4096){t->reason=RA_STATE_LIMIT;return 0;}
    size=rc_trigger_size(definition);
    if(size<=0){t->reason=RA_STATE_PARSE;return 0;}
    if(size>32768 || !(t->buffer=malloc((size_t)size))){t->reason=RA_STATE_LIMIT;return 0;}
    t->trigger=rc_parse_trigger(t->buffer,definition,NULL,0);
    if(!t->trigger){t->reason=RA_STATE_PARSE;return 0;}
    if(!group(t,t->trigger->requirement,&count)){t->reason=RA_STATE_HISTORY;return 0;}
    for(set=t->trigger->alternative;set;set=set->next){
        t->reject_group=++groups;
        if(groups>16){
            t->reject_condition=0;t->reject_operand=0;
            t->reject_type=0;t->reject_operator=0;t->reject_hits=0;
            t->reject_operand_type=0;t->reject_access=0;t->reject_size=0;
            t->reject_indirect=0;t->reject_hittarget=0;t->reject_reset=0;t->reject_measured=0;
            t->reject_detail="groups_limit";t->reason=RA_STATE_HISTORY;return 0;
        }
        if(!group(t,set,&count)){t->reason=RA_STATE_HISTORY;return 0;}
    }
    if(!count || !t->refs_count){t->reason=RA_STATE_UNSUPPORTED;return 0;}
    t->reason=RA_STATE_READY;return 1;
}
static uint32_t peek(uint32_t address,uint32_t size,void* context)
{
    ra_state_test* t=context;unsigned i;
    for(i=0;i<t->refs_count;++i)if(t->refs[i].address==address && t->refs[i].size==size)return t->refs[i].value;
    t->unexpected_peek=1;return 0;
}
int ra_state_test_check(ra_state_test* t,uint32_t (*read)(uint32_t,uint8_t*,uint32_t,void*),void* context)
{
    unsigned pass,i,j;int match;
    if(!t || t->reason!=RA_STATE_READY || !read || !t->refs_count || t->refs_count>RA_STATE_TEST_REFS)return -3;
    ++t->checks;
    for(pass=0;pass<2;++pass)for(i=0;i<t->refs_count;++i){
        uint8_t bytes[4];uint32_t value=0;
        if(!t->refs[i].size || t->refs[i].size>sizeof(bytes))return -3;
        if(read(t->refs[i].address,bytes,t->refs[i].size,context)!=t->refs[i].size){++t->read_errors;return -1;}
        for(j=0;j<t->refs[i].size;++j)value|=(uint32_t)bytes[j]<<(8*j);
        if(pass && value!=t->refs[i].value){++t->unstable;return -2;}
        t->refs[i].value=value;
    }
    rc_reset_trigger(t->trigger);
    t->unexpected_peek=0;
    match=rc_test_trigger(t->trigger,peek,t,NULL);
    if(t->unexpected_peek){++t->read_errors;return -1;}
    if(!match)t->seen_false=1;
    else if(t->seen_false)t->matched=1;
    else t->initial_true=1;
    return match;
}
