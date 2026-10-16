#include "placement2_policy.h"
static int partition_range_valid(uint32_t address, uint32_t size)
{
    return address == UINT32_C(0x08800000) && size && size <= UINT32_C(0x01800000);
}
int ra_placement2_partition_valid(uint32_t address, uint32_t size, uint32_t attributes)
{
    return partition_range_valid(address, size) && (attributes & UINT32_C(3));
}
int ra_placement2_segment_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t address, uint32_t size)
{
    if (!partition_range_valid(partition_address, partition_size) || !size || address < partition_address)
        return 0;
    return address - partition_address < partition_size &&
        size <= partition_size - (address - partition_address);
}
int ra_placement2_can_hold(uint32_t partition_size, uint32_t total, uint32_t largest)
{
    return partition_size && partition_size <= UINT32_C(0x01800000) &&
        total <= partition_size && largest <= total &&
        total >= RA_PLACEMENT2_FREE_FLOOR && largest >= RA_PLACEMENT2_MODULE_BUDGET;
}
int ra_placement2_can_load(uint32_t partition_size, uint32_t total, uint32_t largest)
{
    return ra_placement2_can_hold(partition_size, total, largest) &&
        total - RA_PLACEMENT2_FREE_FLOOR >= RA_PLACEMENT2_MODULE_BUDGET;
}
int ra_placement2_module_sizes_valid(uint32_t text, uint32_t data, uint32_t bss)
{
    return text && data >= UINT32_C(1024) && bss >= UINT32_C(1024) &&
        (uint64_t)text + data + bss <= RA_PLACEMENT2_MODULE_BUDGET;
}
