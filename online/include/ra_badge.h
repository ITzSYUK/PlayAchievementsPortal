#ifndef RA_BADGE_H
#define RA_BADGE_H
#include <stddef.h>
#include <stdint.h>
/* PBAD v1 from tools/compile_badges.py and plugin/src/badge.c. Artwork only:
 * no offline achievement state is read or uploaded by the online client. */
#define RA_BADGE_MAX 128u
#define RA_BADGE_BYTES 2048u
#define RA_BADGE_PACK_LIMIT (48u + RA_BADGE_MAX * (12u + RA_BADGE_BYTES))
int ra_badge_validate(const void*, size_t, const char* disc_id);
const uint16_t* ra_badge_pixels(const void*, size_t, unsigned id);
#endif
