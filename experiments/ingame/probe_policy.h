#ifndef RA_PROBE_POLICY_H
#define RA_PROBE_POLICY_H
#include <stdint.h>
typedef struct { uint32_t address, size; } ra_probe_region;
int ra_probe_region_contains(const ra_probe_region*, unsigned count, uint32_t address, uint32_t length);
int ra_probe_network_allowed(unsigned present, unsigned stopped, unsigned suspended);
#endif
