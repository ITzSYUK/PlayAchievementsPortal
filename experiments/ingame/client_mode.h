#ifndef RA_CLIENT_MODE_H
#define RA_CLIENT_MODE_H
#include <string.h>
static inline int ra_client_encore_parse(const char* bytes,unsigned size)
{
    if(!bytes || !size || size>=8)return -1;
    while(size && (bytes[size-1]=='\r' || bytes[size-1]=='\n'))--size;
    return size==1 && (bytes[0]=='0' || bytes[0]=='1')?bytes[0]-'0':-1;
}
/* Explicit, exact marker only. Invalid/truncated/BOM input never arms awards. */
static inline int ra_client_mode_parse(const char* bytes, unsigned size)
{
    if (!bytes || !size || size >= 16u) return -1;
    while (size && (bytes[size-1] == '\n' || bytes[size-1] == '\r')) --size;
    if (size == 8u && !memcmp(bytes, "softcore", 8)) return 1;
    if (size == 9u && !memcmp(bytes, "spectator", 9)) return 0;
    return -1;
}
/* Observation length only; never changes account/spectator permissions. */
static inline int ra_client_duration_parse(const char* bytes, unsigned size, unsigned* seconds)
{
    unsigned i, value = 0;
    if (!bytes || !seconds || !size || size >= 16u) return 0;
    while (size && (bytes[size-1] == '\n' || bytes[size-1] == '\r')) --size;
    if(size==7u && !memcmp(bytes,"session",7)) { *seconds=0; return 1; }
    if (!size || size > 3u) return 0;
    for (i=0;i<size;++i) {
        if (bytes[i]<'0' || bytes[i]>'9') return 0;
        value=value*10u+(unsigned)(bytes[i]-'0');
    }
    if (value<30u || value>300u) return 0;
    *seconds=value; return 1;
}
#endif
