#include "probe_policy.h"
#include <stdio.h>
#include <stdint.h>
int main(void)
{
    const ra_probe_region regions[] = {{0x08804000u, 4096}, {0x08900000u, 128}};
    const ra_probe_region overflow[] = {{0x08900000u, UINT32_MAX}};
    if (!ra_probe_region_contains(regions, 2, 0x08804000u, 64) ||
        !ra_probe_region_contains(regions, 2, 0x08804FFFu, 1) ||
        !ra_probe_region_contains(regions, 2, 0x08900040u, 64)) return 1;
    if (ra_probe_region_contains(regions, 2, 0x08804FFFu, 2) ||
        ra_probe_region_contains(regions, 2, 0x08805000u, 1) ||
        ra_probe_region_contains(regions, 2, 0x88804000u, 1) ||
        ra_probe_region_contains(regions, 2, 0x087FFFFFu, 1) ||
        ra_probe_region_contains(regions, 2, 0x08900000u, UINT32_MAX) ||
        ra_probe_region_contains(regions, 2, 0x08900000u, 0) ||
        ra_probe_region_contains(overflow, 1, 0x08900000u, 1) ||
        ra_probe_region_contains(NULL, 2, 0x08804000u, 1) ||
        ra_probe_region_contains(regions, 5, 0x08804000u, 1)) return 1;
    if (!ra_probe_network_allowed(0, 0, 0) || ra_probe_network_allowed(1, 0, 0) ||
        ra_probe_network_allowed(0, 1, 0) || ra_probe_network_allowed(0, 0, 1)) return 1;
    puts("PASS: reads confined to module segments; foreign network/suspend/cancel denied");
    return 0;
}
