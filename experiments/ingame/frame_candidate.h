#ifndef RA_FRAME_CANDIDATE_H
#define RA_FRAME_CANDIDATE_H
#include <stdint.h>
/* Diagnostic polling only. Never use these samples to advance rcheevos.
 * The fixed addresses apply solely to the owner's verified UCUS98662 image. */
#define RA_CANDIDATE_COUNTER 0x00ADB21Cu
typedef uint32_t (*ra_candidate_read)(uint32_t,uint8_t*,uint32_t,void*);
typedef struct {
    uint64_t last_ms;
    uint32_t last_value,polls,changes,increments,max_delta,gaps,discontinuities,read_errors;
    unsigned have_value;
} ra_candidate_observation;
/* Exact observed function prefix, not just an address/region check. */
int ra_candidate_signature(ra_candidate_read,void*);
void ra_candidate_reset(ra_candidate_observation*);
int ra_candidate_poll(ra_candidate_observation*,uint64_t,ra_candidate_read,void*);
#endif
