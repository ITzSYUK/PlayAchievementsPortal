#ifndef RA_FRAME_TIMELINE_H
#define RA_FRAME_TIMELINE_H
#include <stdint.h>
/* Display metadata only: no dereference of the framebuffer or game object. */
typedef struct {
    uint32_t us, vcount, framebuffer, stride, format;
    int valid;
} ra_timeline_point;
typedef struct { ra_timeline_point before, after; } ra_timeline_trace;
typedef struct {
    uint32_t calls, duplicate_vcount, skipped_vcount, reset_vcount;
    uint32_t framebuffer_changes, read_errors, crossed_vblank;
    uint32_t duration_max_us, interval_max_us, traces;
    ra_timeline_point previous;
    int have_previous;
    ra_timeline_trace trace[4];
} ra_frame_timeline;
static inline void ra_timeline_add(ra_frame_timeline* s,
    const ra_timeline_point* before, const ra_timeline_point* after)
{
    uint32_t duration=after->us-before->us;
    ++s->calls;
    if(duration>s->duration_max_us)s->duration_max_us=duration;
    if(before->vcount!=after->vcount)++s->crossed_vblank;
    if(!before->valid || !after->valid)++s->read_errors;
    if(s->have_previous){
        uint32_t advance=before->vcount-s->previous.vcount;
        uint32_t interval=before->us-s->previous.us;
        if(!advance)++s->duplicate_vcount;
        else if(advance>0x7FFFFFFFu)++s->reset_vcount;
        else if(advance>1)s->skipped_vcount+=advance-1;
        if(interval>s->interval_max_us)s->interval_max_us=interval;
        if(before->valid && s->previous.valid &&
           (before->framebuffer!=s->previous.framebuffer || before->stride!=s->previous.stride ||
            before->format!=s->previous.format))++s->framebuffer_changes;
    }
    if(s->traces<4){s->trace[s->traces].before=*before;s->trace[s->traces++].after=*after;}
    s->previous=*before;s->have_previous=1;
}
#endif
