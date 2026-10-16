#include "frame_timeline.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    ra_frame_timeline s={0};
    ra_timeline_point a={0xFFFFFFF0u,0xFFFFFFFFu,0x04000000,512,3,1},b=a;
    b.us=0x10;ra_timeline_add(&s,&a,&b);
    assert(s.duration_max_us==32 && s.have_previous);
    a.us=0x100;a.vcount=0;b=a;b.vcount=1;
    ra_timeline_add(&s,&a,&b);
    assert(!s.skipped_vcount && !s.reset_vcount && s.crossed_vblank==1);
    b=a;ra_timeline_add(&s,&a,&b);assert(s.duplicate_vcount==1);
    a.vcount=4;a.framebuffer+=0x88000;b=a;ra_timeline_add(&s,&a,&b);
    assert(s.skipped_vcount==3 && s.framebuffer_changes==1);
    a.vcount=1;a.valid=0;b=a;ra_timeline_add(&s,&a,&b);
    assert(s.reset_vcount==1 && s.read_errors==1 && s.traces==4 && s.calls==5);
    puts("PASS frame timeline: time/vcount wrap, duplicates, skipped ticks, resets, invalid metadata, trace cap");
    return 0;
}
