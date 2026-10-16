#ifndef RA_EXTRA_RAM_POLICY_H
#define RA_EXTRA_RAM_POLICY_H
#include <stdint.h>
#define RA_EXTRA_RESERVE_BYTES UINT32_C(0x00200000)
#define RA_EXTRA_PLACEMENT_UNSUPPORTED 2
int ra_extra_user_partition_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t partition_attributes);
int ra_extra_block_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t block_address, uint32_t block_size);
int ra_extra_slice_valid(uint32_t block_size, uint32_t offset, uint32_t length);
int ra_extra_user_segment_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t segment_address, uint32_t segment_size);
#endif
