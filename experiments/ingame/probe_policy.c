#include "probe_policy.h"
int ra_probe_region_contains(const ra_probe_region* regions, unsigned count, uint32_t address, uint32_t length)
{
    unsigned i;
    if (!regions || !count || count > 4 || !length || address < 0x08800000u ||
        address >= 0x0A000000u || length > 0x0A000000u - address) return 0;
    for (i = 0; i < count; ++i) {
        uint32_t begin = regions[i].address, size = regions[i].size;
        if (begin < 0x08800000u || begin >= 0x0A000000u || !size || size > 0x0A000000u - begin)
            continue;
        if (address >= begin && address - begin < size && length <= size - (address - begin)) return 1;
    }
    return 0;
}
int ra_probe_network_allowed(unsigned present, unsigned stopped, unsigned suspended)
{ return !present && !stopped && !suspended; }
