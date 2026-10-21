#ifndef RA_PENDING_H
#define RA_PENDING_H
#include <stddef.h>
#include <stdint.h>
#define RA_PENDING_MAX 128u
#define RA_PENDING_HEADER 128u
#define RA_PENDING_RECORD 28u
#define RA_PENDING_WIRE_MAX (RA_PENDING_HEADER + RA_PENDING_MAX*RA_PENDING_RECORD + 32u)
#define RA_PENDING_MAX_AGE (7u*24u*60u*60u)
typedef struct { uint32_t id, native; uint64_t earned; unsigned char definition[16]; } ra_pending_entry;
typedef struct {
    uint32_t generation, game_id, count;
    char account[65], hash[33];
    ra_pending_entry entry[RA_PENDING_MAX];
} ra_pending;
/* MAC is HMAC-SHA256 in production, keyed by the current session token.
 * Signing is worker-only; neither key nor token appears in the wire format. */
typedef int (*ra_pending_mac)(void*,const void*,size_t,unsigned char[32]);
int ra_pending_identity(ra_pending*,const char*,const char*,unsigned);
size_t ra_pending_encode(const ra_pending*,void*,size_t,ra_pending_mac,void*);
int ra_pending_decode(ra_pending*,const void*,size_t,const ra_pending*,ra_pending_mac,void*);
int ra_pending_add(ra_pending*,unsigned,uint64_t,unsigned);
void ra_pending_remove(ra_pending*,unsigned);
#endif
