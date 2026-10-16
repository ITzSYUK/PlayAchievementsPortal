#ifndef RA_UI_H
#define RA_UI_H
#include <stdint.h>
#define RA_UI_MAX 128u
#define RA_UI_NOTICES 16u
enum { RA_UI_LOCKED, RA_UI_CONFIRMED, RA_UI_PENDING, RA_UI_LOCAL,
       RA_UI_REJECTED, RA_UI_UNSUPPORTED };
typedef struct {
    unsigned id, points, state;
    char title[96], description[256];
} ra_ui_row;
typedef struct { unsigned type, id, time_ms; } ra_ui_notice;
typedef struct {
    /* Metadata immutable after prepare, until engine_stop. State words use
     * atomic load/store. One serialized rc_client producer, one UI consumer. */
    ra_ui_row rows[RA_UI_MAX];
    unsigned count, spectator, encore;
    unsigned write, read, dropped;
    ra_ui_notice notices[RA_UI_NOTICES];
} ra_ui;
void ra_ui_init(ra_ui*, unsigned spectator);
void ra_ui_publish(ra_ui*, unsigned type, unsigned id, unsigned time_ms);
int ra_ui_take(ra_ui*, ra_ui_notice*);
unsigned ra_ui_state(const ra_ui*, unsigned index);
/* filter: 0 all, 1 unconfirmed, 2 server-confirmed. */
unsigned ra_ui_count(const ra_ui*, unsigned filter);
int ra_ui_index(const ra_ui*, unsigned filter, unsigned position);
const char* ra_ui_label(unsigned state);
typedef struct { void* pixels; unsigned stride, format; } ra_ui_surface;
/* Exactly 480x272, formats 565/5551/4444/8888 (0..3), stride 480..512.
 * Caller validates backing storage and display ownership before drawing. */
int ra_ui_surface_valid(const ra_ui_surface*);
void ra_ui_draw(const ra_ui*, const ra_ui_surface*, int menu, unsigned filter,
                unsigned selected, const ra_ui_notice* notice);
#define RA_NOTICE_X 112u
#define RA_NOTICE_Y 205u
#define RA_NOTICE_WIDTH 360u
#define RA_NOTICE_HEIGHT 59u
/* Copy an already composed RGB565 card to a validated, not-yet-displayed
 * surface. No allocation, strings, waits or graphics list changes. */
void ra_ui_blit_notice(const uint16_t*,const ra_ui_surface*);
#endif
