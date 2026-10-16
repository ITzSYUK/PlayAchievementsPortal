#include "frame_candidate.h"
#include <string.h>
static uint32_t word(const uint8_t* p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int ra_candidate_signature(ra_candidate_read read,void* context)
{
    static const uint32_t prefix[]={0x27BDFFF0,0x3C0308AE,0x8C65B21C,0xAFBF000C,
        0x3C0408AE,0x3C030897,0x906319C0,0x24A50001,0x14600008,0xAC85B21C,
        0x0E2572C9,0x24040001};
    uint8_t bytes[sizeof(prefix)];unsigned i;
    if(!read || read(0x008428B0,bytes,sizeof(bytes),context)!=sizeof(bytes))return 0;
    for(i=0;i<sizeof(prefix)/sizeof(*prefix);++i)if(word(bytes+i*4)!=prefix[i])return 0;
    return 1;
}
void ra_candidate_reset(ra_candidate_observation* o)
{ memset(o,0,sizeof(*o)); }
int ra_candidate_poll(ra_candidate_observation* o,uint64_t now,ra_candidate_read read,void* context)
{
    uint8_t bytes[4];uint32_t value,delta;
    ++o->polls;
    if(!read || read(RA_CANDIDATE_COUNTER,bytes,4,context)!=4){
        ++o->read_errors;o->have_value=0;return 0;
    }
    value=word(bytes);
    if(o->have_value){
        if(now<o->last_ms || now-o->last_ms>250u)++o->gaps;
        else {
            delta=value-o->last_value; /* Unsigned wrap is intentional. */
            /* A diagnostic discontinuity guard, not an assumed frame rate.
             * Never attribute a reset/huge jump to 4 billion game frames. */
            if(delta>1024u || delta>UINT32_MAX-o->increments)++o->discontinuities;
            else {
                o->increments+=delta;
                if(delta)++o->changes;
                if(delta>o->max_delta)o->max_delta=delta;
            }
        }
    }
    o->last_value=value;o->last_ms=now;o->have_value=1;return 1;
}
