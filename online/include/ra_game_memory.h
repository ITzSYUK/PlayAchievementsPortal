#ifndef RA_GAME_MEMORY_H
#define RA_GAME_MEMORY_H
#include "ra_psp_memory.h"
#define RA_GAME_SEGMENTS 4u
#define RA_GAME_READ_SAMPLES 16u
typedef struct { uint32_t address, size; } ra_game_region;
typedef struct { uint32_t count; ra_game_region regions[RA_GAME_SEGMENTS]; } ra_game_map;
typedef struct { uint32_t address, size; unsigned char value[4]; } ra_game_sample;
typedef struct {
    ra_game_map map;
    ra_psp_platform_read_t read;
    void* context;
    uint32_t calls, accepted, rejected, failed;
    uint32_t sample_count, denied_count;
    ra_game_sample samples[RA_GAME_READ_SAMPLES];
    ra_game_region denied[RA_GAME_READ_SAMPLES];
} ra_game_reader;
/* Deliberately covers only the original 24 MiB user region, and only loaded
 * game segments. Kernel RAM, aliases, dynamic heaps and extra RAM are denied. */
int ra_game_map_valid(const ra_game_map*);
int ra_game_map_contains(const ra_game_map*, uint32_t physical, uint32_t size);
int ra_game_reader_init(ra_game_reader*, const ra_game_map*, ra_psp_platform_read_t, void*);
uint32_t ra_game_memory_read(uint32_t ra_address, uint8_t*, uint32_t size, void*);
/* Diagnostic rereads of the first accepted addresses; no values leave here.
 * Caller must stop using the reader before game/module teardown. */
int ra_game_memory_sample(ra_game_reader*, uint32_t* changed);
#endif
