#include "extra_ram_policy.h"
int ra_extra_user_partition_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t partition_attributes)
{
    /* ModuleMgr's user-partition check requires at least one of the low two
     * attribute bits. This preflight does not grant DDR access or execution. */
    return (partition_attributes & UINT32_C(3)) &&
        !(partition_address & UINT32_C(0x80000000)) &&
        ra_extra_block_valid(partition_address, partition_size, partition_address, partition_size);
}
int ra_extra_block_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t block_address, uint32_t block_size)
{
    const uint32_t base = partition_address & UINT32_C(0x7FFFFFFF);
    const uint32_t address = block_address & UINT32_C(0x7FFFFFFF);
    /* Only the partition 9 layout observed on this PSP is accepted, with
     * its ordinary address or cached kernel alias. No partition 2 fallback. */
    if (base != UINT32_C(0x0A000000) || !partition_size ||
        partition_size > UINT32_C(0x01800000) || !block_size || address < base)
        return 0;
    return address - base < partition_size && block_size <= partition_size - (address - base);
}
int ra_extra_slice_valid(uint32_t block_size, uint32_t offset, uint32_t length)
{
    return length && offset < block_size && length <= block_size - offset;
}
int ra_extra_user_segment_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t segment_address, uint32_t segment_size)
{
    /* User code cannot execute from a cached kernel alias. Unlike kernel
     * scratch blocks, module segment addresses must retain their user form. */
    return !(segment_address & UINT32_C(0x80000000)) &&
        ra_extra_block_valid(partition_address, partition_size, segment_address, segment_size);
}
