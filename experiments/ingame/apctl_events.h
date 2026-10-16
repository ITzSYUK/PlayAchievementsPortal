#ifndef RA_APCTL_EVENTS_H
#define RA_APCTL_EVENTS_H
#include <stdint.h>
#include <string.h>
#define RA_APCTL_EVENTS_MAX 16u
typedef struct { uint32_t when_ms; int old_state, new_state, event, error; } ra_apctl_event;
typedef struct {
    ra_apctl_event events[RA_APCTL_EVENTS_MAX];
    unsigned head, count, dropped;
    int first_error;
} ra_apctl_events;
/* Caller holds the same semaphore for push/pop. No I/O or allocation here. */
static inline void ra_apctl_events_init(ra_apctl_events* q) { memset(q, 0, sizeof(*q)); }
static inline void ra_apctl_events_push(ra_apctl_events* q, const ra_apctl_event* e)
{
    /* Preserve the first native error even if the bounded history overflows. */
    if (!q->first_error && e->error) q->first_error = e->error;
    if (q->count == RA_APCTL_EVENTS_MAX) { if (q->dropped != UINT32_MAX) ++q->dropped; return; }
    q->events[(q->head + q->count) % RA_APCTL_EVENTS_MAX] = *e; ++q->count;
}
static inline int ra_apctl_events_pop(ra_apctl_events* q, ra_apctl_event* e)
{
    if (!q->count) return 0;
    *e = q->events[q->head]; q->head = (q->head + 1) % RA_APCTL_EVENTS_MAX; --q->count;
    return 1;
}
#endif
