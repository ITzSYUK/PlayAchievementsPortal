#include "ra_auth_file.h"
#include <string.h>
void ra_auth_erase(void* data, size_t size)
{
    volatile unsigned char* p = data;
    while (size--) *p++ = 0;
}
static int line(const unsigned char** data, size_t* size, char* out, size_t cap)
{
    size_t n = 0;
    while (*size && **data != '\r' && **data != '\n') {
        unsigned c = **data;
        if (c < 33 || c > 126 || n + 1 >= cap) return 0;
        out[n++] = (char)c; ++*data; --*size;
    }
    out[n] = 0;
    if (*size && **data == '\r') { ++*data; --*size; if (!*size || **data != '\n') return 0; }
    if (*size && **data == '\n') { ++*data; --*size; }
    return n != 0;
}
int ra_auth_parse(const void* data, size_t size, ra_auth* auth)
{
    const unsigned char* p = data;
    char marker[24];
    if (!auth) return 0;
    ra_auth_erase(auth, sizeof(*auth));
    if (!p || !size || size > RA_AUTH_FILE_LIMIT ||
        !line(&p, &size, marker, sizeof(marker)) || strcmp(marker, "RAClientToken/1") ||
        !line(&p, &size, auth->username, sizeof(auth->username)) ||
        !line(&p, &size, auth->token, sizeof(auth->token)) || size) {
        ra_auth_erase(auth, sizeof(*auth)); return 0;
    }
    return 1;
}
