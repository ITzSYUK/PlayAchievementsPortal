#include "ra_psp_hash.h"

#include "md5.h"
#include <stdio.h>

#define RA_PSP_HASH_FILE_LIMIT (64L * 1024L * 1024L)

static int append_file(md5_state_t* md5, const char* path)
{
    unsigned char buffer[2048];
    FILE* file;
    long remaining;
    int ok = 1;

    file = fopen(path, "rb");
    if (file == NULL)
        return 0;
    if (fseek(file, 0, SEEK_END) != 0 || (remaining = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    if (remaining > RA_PSP_HASH_FILE_LIMIT)
        remaining = RA_PSP_HASH_FILE_LIMIT;
    while (remaining > 0) {
        const size_t size = remaining > (long)sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
        if (fread(buffer, 1, size, file) != size) {
            ok = 0;
            break;
        }
        md5_append(md5, buffer, (int)size);
        remaining -= (long)size;
    }
    if (fclose(file) != 0)
        ok = 0;
    return ok;
}

int ra_psp_hash_disc_files(char hash[33], const char* param_sfo,
    const char* eboot_bin)
{
    static const char hex[] = "0123456789abcdef";
    md5_state_t md5;
    unsigned char digest[16];
    unsigned int i;

    if (hash == NULL)
        return 0;
    hash[0] = '\0';
    if (param_sfo == NULL || eboot_bin == NULL)
        return 0;
    md5_init(&md5);
    if (!append_file(&md5, param_sfo) || !append_file(&md5, eboot_bin))
        return 0;
    md5_finish(&md5, digest);
    for (i = 0; i < 16; ++i) {
        hash[i * 2] = hex[digest[i] >> 4];
        hash[i * 2 + 1] = hex[digest[i] & 15];
    }
    hash[32] = '\0';
    return 1;
}
