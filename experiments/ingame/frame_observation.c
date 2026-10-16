#include "frame_observation.h"
#include <string.h>
void ra_frame_observation_reset(ra_frame_observation* o, uint64_t now, uint32_t vcount)
{
    memset(o, 0, sizeof(*o)); o->started = o->last = now;
    o->first_vcount = o->last_vcount = vcount;
}
void ra_frame_observation_poll(ra_frame_observation* o, uint64_t now, uint32_t vcount,
    int valid, uintptr_t buffer)
{
    if (now < o->last || now - o->last > 250u) {
        ++o->gaps; o->have_buffer = 0;
    }
    o->last = now; o->last_vcount = vcount; ++o->polls;
    if (!valid || !buffer) { ++o->errors; o->have_buffer = 0; return; }
    if (o->have_buffer && o->buffer != buffer) ++o->changes;
    o->buffer = buffer; o->have_buffer = 1;
}
