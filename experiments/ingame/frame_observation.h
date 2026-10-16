#ifndef RA_FRAME_OBSERVATION_H
#define RA_FRAME_OBSERVATION_H
#include <stdint.h>
/* Observations only: neither vcount nor framebuffer changes are game frames.
 * No do_frame or condition evaluation is permitted from these counters. */
typedef struct {
    uint64_t started, last;
    uintptr_t buffer;
    uint32_t first_vcount, last_vcount, polls, changes, errors, gaps;
    int have_buffer;
} ra_frame_observation;
void ra_frame_observation_reset(ra_frame_observation*, uint64_t now, uint32_t vcount);
void ra_frame_observation_poll(ra_frame_observation*, uint64_t now, uint32_t vcount,
    int valid, uintptr_t buffer);
#endif
