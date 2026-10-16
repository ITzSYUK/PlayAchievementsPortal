#include "ra_game_memory.h"
#include <string.h>
static int region_valid(uint32_t address, uint32_t size)
{
    return size && address >= UINT32_C(0x08800000) && address < UINT32_C(0x0A000000) &&
        size <= UINT32_C(0x0A000000) - address;
}
int ra_game_map_valid(const ra_game_map* map)
{
    unsigned i, j;
    if (!map || !map->count || map->count > RA_GAME_SEGMENTS) return 0;
    for (i = 0; i < map->count; ++i) {
        const ra_game_region* r = &map->regions[i];
        if (!region_valid(r->address, r->size)) return 0;
        for (j = 0; j < i; ++j) {
            const ra_game_region* p = &map->regions[j];
            if (r->address < p->address + p->size && p->address < r->address + r->size) return 0;
        }
    }
    return 1;
}
int ra_game_map_contains(const ra_game_map* map, uint32_t physical, uint32_t size)
{
    unsigned i;
    if (!region_valid(physical, size) || !ra_game_map_valid(map)) return 0;
    for (i = 0; i < map->count; ++i) {
        const ra_game_region* r = &map->regions[i];
        if (physical >= r->address && physical - r->address < r->size &&
            size <= r->size - (physical - r->address)) return 1;
    }
    return 0;
}
int ra_game_reader_init(ra_game_reader* reader, const ra_game_map* map,
    ra_psp_platform_read_t read, void* context)
{
    if (!reader) return 0;
    memset(reader, 0, sizeof(*reader));
    if (!read || !ra_game_map_valid(map)) return 0;
    reader->map = *map; reader->read = read; reader->context = context;
    return 1;
}
static void denied(ra_game_reader* reader, uint32_t address, uint32_t size)
{
    unsigned i;
    ++reader->rejected;
    for (i = 0; i < reader->denied_count; ++i)
        if (reader->denied[i].address == address && reader->denied[i].size == size) return;
    if (reader->denied_count < RA_GAME_READ_SAMPLES) {
        ra_game_region* r = &reader->denied[reader->denied_count++];
        r->address = address; r->size = size;
    }
}
uint32_t ra_game_memory_read(uint32_t address, uint8_t* bytes, uint32_t size, void* context)
{
    ra_game_reader* reader = context;
    uint32_t physical;
    unsigned i;
    if (!reader) return 0;
    ++reader->calls;
    if (!bytes || !reader->read || !ra_psp_memory_translate(address, size, RA_PSP_RAM_32_MB, &physical) ||
        !ra_game_map_contains(&reader->map, physical, size)) {
        denied(reader, address, size); return 0;
    }
    if (reader->read(physical, bytes, size, reader->context) != size) { ++reader->failed; return 0; }
    ++reader->accepted;
    if (size <= 4) {
        for (i = 0; i < reader->sample_count; ++i)
            if (reader->samples[i].address == address && reader->samples[i].size == size) return size;
        if (reader->sample_count < RA_GAME_READ_SAMPLES) {
            ra_game_sample* sample = &reader->samples[reader->sample_count++];
            sample->address = address; sample->size = size; memcpy(sample->value, bytes, size);
        }
    }
    return size;
}
int ra_game_memory_sample(ra_game_reader* reader, uint32_t* changed)
{
    unsigned i;
    if (!reader || !changed || !reader->read || reader->sample_count > RA_GAME_READ_SAMPLES ||
        !ra_game_map_valid(&reader->map)) return 0;
    *changed = 0;
    for (i = 0; i < reader->sample_count; ++i) {
        const uint32_t address = reader->samples[i].address, size = reader->samples[i].size;
        uint32_t physical;
        uint8_t bytes[4];
        if (size > sizeof(bytes) || !ra_psp_memory_translate(address, size, RA_PSP_RAM_32_MB, &physical) ||
            !ra_game_map_contains(&reader->map, physical, size) ||
            reader->read(physical, bytes, size, reader->context) != size) return 0;
        if (memcmp(bytes, reader->samples[i].value, size)) ++*changed;
        memcpy(reader->samples[i].value, bytes, size);
    }
    return 1;
}
