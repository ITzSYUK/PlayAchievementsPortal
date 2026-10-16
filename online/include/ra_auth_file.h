#ifndef RA_AUTH_FILE_H
#define RA_AUTH_FILE_H
#include <stddef.h>
#define RA_AUTH_FILE_LIMIT 256u
typedef struct { char username[65], token[129]; } ra_auth;
/* Strict, bounded ASCII format. No logging and no passwords/API keys. */
int ra_auth_parse(const void*, size_t, ra_auth*);
void ra_auth_erase(void*, size_t);
#endif
