#include "ra_psp_hash.h"
#include "rc_hash.h"

#include <stdio.h>
#include <string.h>

/* The pinned upstream hash loop can finish after a later short sector read.
 * This single-process diagnostic rejects that result instead of printing a
 * plausible hash for a truncated image. The upstream source stays unchanged. */
static rc_hash_cdreader_t default_cdreader;
static int short_read;

static size_t RC_CCONV strict_read_sector(void* track, uint32_t sector,
    void* buffer, size_t size)
{
    const size_t actual = default_cdreader.read_sector(track, sector, buffer, size);
    if (actual != size)
        short_read = 1;
    return actual;
}

static void RC_CCONV hash_error(const char* message, const rc_hash_iterator_t* iterator)
{
    (void)iterator;
    fprintf(stderr, "Hash error: %s\n", message);
}

int main(int argc, char** argv)
{
    char hash[33];
    if (argc == 3 && strcmp(argv[1], "--iso") == 0) {
        rc_hash_iterator_t iterator;
        const size_t length = strlen(argv[2]);
        int ok;
        if (length < 4 || argv[2][length - 4] != '.' ||
            (argv[2][length - 3] != 'i' && argv[2][length - 3] != 'I') ||
            (argv[2][length - 2] != 's' && argv[2][length - 2] != 'S') ||
            (argv[2][length - 1] != 'o' && argv[2][length - 1] != 'O')) {
            fprintf(stderr, "This probe supports uncompressed ISO images only\n");
            return 2;
        }
        rc_hash_initialize_iterator(&iterator, argv[2], NULL, 0);
        rc_hash_get_default_cdreader(&default_cdreader);
        iterator.callbacks.cdreader = default_cdreader;
        iterator.callbacks.cdreader.read_sector = strict_read_sector;
        iterator.callbacks.error_message = hash_error;
        ok = rc_hash_generate(hash, RC_CONSOLE_PSP, &iterator);
        rc_hash_destroy_iterator(&iterator);
        if (!ok || short_read) {
            fprintf(stderr, "Cannot hash complete PSP ISO\n");
            return 1;
        }
    }
    else if (argc == 4 && strcmp(argv[1], "--files") == 0) {
        if (!ra_psp_hash_disc_files(hash, argv[2], argv[3])) {
            fprintf(stderr, "Cannot hash both original disc files\n");
            return 1;
        }
    }
    else {
        fprintf(stderr, "Usage: hash_probe --iso GAME.iso | --files PARAM.SFO EBOOT.BIN\n");
        return 2;
    }
    puts(hash);
    return 0;
}
