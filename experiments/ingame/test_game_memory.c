#include "ra_game_memory.h"
#include "frame_observation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned reads, fail, value;
static uint32_t backend(uint32_t address, uint8_t* bytes, uint32_t size, void* unused)
{
    (void)unused; ++reads;
    assert((address >= 0x08804000 && size <= 0x08805000 - address) ||
           (address >= 0x08900000 && size <= 0x08901000 - address));
    memset(bytes, value, size); return fail ? size - 1 : size;
}
int main(void)
{
    ra_game_map map = { 2, { {0x08804000,4096}, {0x08900000,4096} } }, bad;
    ra_game_reader reader;
    ra_frame_observation o;
    uint8_t bytes[8]; uint32_t changed; unsigned before, i;
    assert(ra_game_map_valid(&map));
    assert(ra_game_map_contains(&map,0x08804000,1) && ra_game_map_contains(&map,0x08804FFC,4));
    assert(!ra_game_map_contains(&map,0x08804FFC,5) && !ra_game_map_contains(&map,0x08805000,1));
    bad=map;bad.count=5;assert(!ra_game_map_valid(&bad));
    bad=map;bad.regions[1].address=0x08804001;assert(!ra_game_map_valid(&bad));
    bad=map;bad.regions[1].address=0xFFFFFFFF;assert(!ra_game_map_valid(&bad));
    bad=map;bad.regions[1].size=0;assert(!ra_game_map_valid(&bad));
    assert(!ra_game_reader_init(&reader,&bad,backend,NULL) && !reader.read);
    assert(ra_game_reader_init(&reader,&map,backend,NULL));
    value=7;
    assert(ra_game_memory_read(0x00804000,bytes,4,&reader)==4 && bytes[0]==7 && reads==1);
    assert(ra_game_memory_read(0x00804FFC,bytes,4,&reader)==4 && reads==2);
    before=reads; memset(bytes,0xCC,sizeof(bytes));
    assert(!ra_game_memory_read(0x00804FFC,bytes,5,&reader));
    assert(!ra_game_memory_read(0,bytes,1,&reader));
    assert(!ra_game_memory_read(0x02000000,bytes,1,&reader));
    assert(!ra_game_memory_read(0x88804000,bytes,1,&reader));
    assert(!ra_game_memory_read(UINT32_MAX,bytes,4,&reader));
    assert(!ra_game_memory_read(0x00804000,NULL,1,&reader));
    assert(!ra_game_memory_read(0x00804000,bytes,0,&reader));
    assert(reads==before && bytes[0]==0xCC);
    fail=1;assert(!ra_game_memory_read(0x00804000,bytes,4,&reader) && reader.failed==1);
    fail=0;assert(ra_game_memory_sample(&reader,&changed) && !changed);
    value=9;assert(ra_game_memory_sample(&reader,&changed) && changed==2);
    fail=1;assert(!ra_game_memory_sample(&reader,&changed));fail=0;
    for (i=0;i<40;++i) { assert(!ra_game_memory_read(i,bytes,1,&reader));
        assert(ra_game_memory_read(0x00804000+i,bytes,1,&reader)==1); }
    assert(reader.denied_count==RA_GAME_READ_SAMPLES && reader.sample_count==RA_GAME_READ_SAMPLES);
    before=reads;reader.samples[0].size=5;
    assert(!ra_game_memory_sample(&reader,&changed) && reads==before);
    reader.samples[0].size=4;reader.sample_count=RA_GAME_READ_SAMPLES+1;
    assert(!ra_game_memory_sample(&reader,&changed) && reads==before);
    ra_frame_observation_reset(&o,1000,UINT32_MAX-1);
    ra_frame_observation_poll(&o,1005,UINT32_MAX,1,0x04000000);
    ra_frame_observation_poll(&o,1010,0,1,0x04088000);
    ra_frame_observation_poll(&o,1015,1,1,0x04088000);
    assert(o.changes==1 && o.last_vcount-o.first_vcount==3 && o.polls==3);
    ra_frame_observation_poll(&o,2000,60,1,0x04000000);
    assert(o.gaps==1 && o.changes==1);
    ra_frame_observation_poll(&o,2005,61,0,0);
    ra_frame_observation_poll(&o,2010,62,1,0x04088000);
    assert(o.errors==1 && o.changes==1);
    ra_frame_observation_reset(&o,2010,62);
    assert(!o.changes && !o.gaps && !o.polls && !o.have_buffer);
    puts("PASS game memory: official RA mapping, segment bounds, gaps/alias/kernel/extra/overflow denied before read, partial failure, bounded samples; display observations wrap/gap/error/reset");
    return 0;
}
