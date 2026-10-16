#ifndef RA_UI_DISPLAY_H
#define RA_UI_DISPLAY_H
#include <stdint.h>
/* Worker-only presentation. Never draw into the displayed buffer. A buffer
 * cannot be freed until BOTH current and queued display references leave it. */
typedef struct { void* pixels; int stride,format; } ra_ui_fb;
typedef struct {
    void* context;
    int (*get)(void*,ra_ui_fb*,int next);
    int (*set)(void*,const ra_ui_fb*);
} ra_ui_display_api;
typedef struct {
    ra_ui_fb saved,buffer[2];
    int active,front,pending;
} ra_ui_display;
static inline int ra_ui_fb_equal(const ra_ui_fb* a,const ra_ui_fb* b)
{
    return (((uintptr_t)a->pixels&~(uintptr_t)0x40000000u)==
            ((uintptr_t)b->pixels&~(uintptr_t)0x40000000u)) &&
           a->stride==b->stride && a->format==b->format;
}
static inline int ra_ui_display_owns(const ra_ui_display* d,const ra_ui_fb* fb)
{
    unsigned i;uintptr_t address=(uintptr_t)fb->pixels&~(uintptr_t)0x40000000u;
    for(i=0;i<2;++i) {
        uintptr_t base=(uintptr_t)d->buffer[i].pixels&~(uintptr_t)0x40000000u;
        /* Even a foreign format/stride or an interior scanout pointer keeps
         * the allocation live. Equality alone is insufficient for freeing. */
        if(base && address>=base && address-base<512u*272u*2u)return 1;
    }
    return 0;
}
static inline int ra_ui_display_begin(ra_ui_display* d,const ra_ui_display_api* api)
{
    ra_ui_fb next;
    if(api->get(api->context,&d->saved,0)<0 || !d->saved.pixels ||
       api->get(api->context,&next,1)<0)return -1;
    /* Drain a previously queued game flip before claiming the display. */
    if(next.pixels && !ra_ui_fb_equal(&next,&d->saved))return 0;
    d->active=1;d->front=d->pending=-1;return 1;
}
static inline int ra_ui_display_refresh(ra_ui_display* d,const ra_ui_display_api* api)
{
    ra_ui_fb current,next;
    if(api->get(api->context,&current,0)<0 || api->get(api->context,&next,1)<0)return -1;
    if(d->pending>=0 && ra_ui_fb_equal(&current,&d->buffer[d->pending])) {
        d->front=d->pending;d->pending=-1;
    }
    if(!ra_ui_fb_equal(&current,d->front<0?&d->saved:&d->buffer[d->front]))return -1;
    if(next.pixels && !ra_ui_fb_equal(&next,d->pending<0?&current:&d->buffer[d->pending]))return -1;
    return 0;
}
static inline int ra_ui_display_back(const ra_ui_display* d)
{ return d->pending>=0?-1:d->front==0?1:0; }
static inline int ra_ui_display_present(ra_ui_display* d,const ra_ui_display_api* api,int back)
{
    if(back!=ra_ui_display_back(d) || back<0)return -1;
    if(api->set(api->context,&d->buffer[back])<0)return -1;
    d->pending=back;return 0;
}
/* 1: no live references (safe to release game/free); 0: wait for VBlank;
 * -1: uncertain ownership, retain allocation. Respect a foreign HOME/game flip. */
static inline int ra_ui_display_end(ra_ui_display* d,const ra_ui_display_api* api)
{
    ra_ui_fb current,next,target;
    if(!d->active)return 1;
    if(api->get(api->context,&current,0)<0 || api->get(api->context,&next,1)<0)return -1;
    if(!ra_ui_display_owns(d,&current) && !ra_ui_display_owns(d,&next)) {
        d->active=0;d->front=d->pending=-1;return 1;
    }
    if(next.pixels && !ra_ui_display_owns(d,&next))return 0; /* already retiring */
    target=ra_ui_display_owns(d,&current)?d->saved:current;
    if(!target.pixels || api->set(api->context,&target)<0)return -1;
    return 0;
}
#endif
