#ifndef RA_STATE_TEST_H
#define RA_STATE_TEST_H
#include "rc_runtime_types.h"
#define RA_STATE_TEST_REFS 16u
enum { RA_STATE_NOT_FOUND, RA_STATE_READY, RA_STATE_LIMIT, RA_STATE_PARSE,
       RA_STATE_HISTORY, RA_STATE_UNSUPPORTED };
typedef struct { uint32_t address,size,value; } ra_state_ref;
typedef struct {
    void* buffer;
    rc_trigger_t* trigger;
    ra_state_ref refs[RA_STATE_TEST_REFS];
    /* First rejected construct only; no definition text or credential data. */
    const char* reject_detail;
    unsigned reject_group,reject_condition,reject_operand;
    unsigned reject_type,reject_operator,reject_hits,reject_operand_type,reject_access,reject_size;
    unsigned reject_indirect,reject_hittarget,reject_reset,reject_measured;
    unsigned reason,refs_count,checks,seen_false,matched,initial_true,read_errors,unstable,unexpected_peek;
} ra_state_test;
/* Zero-initialize before first prepare. Private copy of one official definition. No rc_client game
 * state, events, awards or frame counters are touched by these methods. */
int ra_state_test_prepare(ra_state_test*, const char* definition);
void ra_state_test_destroy(ra_state_test*);
/* 0 false, 1 current state true, -1 read failure, -2 unstable snapshot,
 * -3 ineligible. Two equal read passes are a consistency check, not proof
 * of an atomic game snapshot or frame synchronization. */
int ra_state_test_check(ra_state_test*,uint32_t (*read)(uint32_t,uint8_t*,uint32_t,void*),void*);
const char* ra_state_reason(unsigned);
#endif
