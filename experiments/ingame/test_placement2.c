#include "placement2_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const uint32_t base = UINT32_C(0x08800000), size = UINT32_C(0x01800000);
    /* Recorded partition 2 at sample 10; the old minimum is also acceptable. */
    assert(ra_placement2_partition_valid(base, size, 0x0Fu));
    assert(ra_placement2_can_load(size, 864512u, 864512u));
    assert(ra_placement2_can_load(size, 569600u, 569600u));
    assert(!ra_placement2_partition_valid(0x0A000000u, size, 0x0Fu));
    assert(!ra_placement2_partition_valid(0x88800000u, size, 0x0Fu));
    assert(!ra_placement2_partition_valid(base, size, 0x0Cu));
    assert(!ra_placement2_partition_valid(base, 0, 0x0Fu));
    assert(!ra_placement2_partition_valid(base, UINT32_MAX, 0x0Fu));
    assert(ra_placement2_segment_valid(base, size, 0x08804000u, 2220u));
    assert(ra_placement2_segment_valid(base, size, 0x09FFFFFFu, 1));
    assert(!ra_placement2_segment_valid(base, size, 0x09FFFFFFu, 2));
    assert(!ra_placement2_segment_valid(base, size, 0x88804000u, 2220u));
    assert(!ra_placement2_segment_valid(base, size, 0x0A000000u, 1));
    assert(!ra_placement2_segment_valid(base, size, 0x08804000u, UINT32_MAX));
    assert(!ra_placement2_segment_valid(base, size, 0, 1));
    assert(!ra_placement2_segment_valid(base, size, base, 0));
    assert(ra_placement2_can_load(size, RA_PLACEMENT2_FREE_FLOOR + RA_PLACEMENT2_MODULE_BUDGET,
        RA_PLACEMENT2_MODULE_BUDGET));
    assert(!ra_placement2_can_load(size, RA_PLACEMENT2_FREE_FLOOR + RA_PLACEMENT2_MODULE_BUDGET - 1,
        RA_PLACEMENT2_MODULE_BUDGET));
    assert(!ra_placement2_can_load(size, 864512u, RA_PLACEMENT2_MODULE_BUDGET - 1));
    assert(!ra_placement2_can_load(size, 864512u, 864513u));
    assert(!ra_placement2_can_load(size, UINT32_MAX, UINT32_MAX));
    assert(ra_placement2_can_hold(size, RA_PLACEMENT2_FREE_FLOOR, RA_PLACEMENT2_MODULE_BUDGET));
    assert(!ra_placement2_can_hold(size, RA_PLACEMENT2_FREE_FLOOR - 1, RA_PLACEMENT2_MODULE_BUDGET));
    assert(ra_placement2_module_sizes_valid(172, 1024, 1024));
    assert(!ra_placement2_module_sizes_valid(0, 1024, 1024));
    assert(!ra_placement2_module_sizes_valid(172, 0, 1024));
    assert(!ra_placement2_module_sizes_valid(172, 1024, 0));
    assert(!ra_placement2_module_sizes_valid(UINT32_MAX, UINT32_MAX, UINT32_MAX));
    assert(ra_placement2_module_sizes_valid(RA_PLACEMENT2_MODULE_BUDGET - 2048, 1024, 1024));
    assert(!ra_placement2_module_sizes_valid(RA_PLACEMENT2_MODULE_BUDGET - 2047, 1024, 1024));
    puts("PASS placement2: recorded memory accepted, bounds/aliases/fragmentation/low memory/size overflow rejected");
    return 0;
}
