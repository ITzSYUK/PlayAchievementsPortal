#include "extra_ram_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    /* Recorded 0.6 partition 9: ample RAM, but incompatible user attributes.
     * User attributes alone must not authorize partition 2 or a kernel alias. */
    assert(!ra_extra_user_partition_valid(0x0A000000u, 25165824u, 0x0Cu));
    assert(!ra_extra_user_partition_valid(0x08800000u, 25165824u, 0x0Fu));
    assert(!ra_extra_user_partition_valid(0x8A000000u, 25165824u, 0x0Fu));
    assert(!ra_extra_user_partition_valid(0x0A000000u, UINT32_MAX, 0x0Fu));
    assert(!ra_extra_user_partition_valid(0x0A000000u, 0, 0x0Fu));
    assert(ra_extra_user_partition_valid(0x0A000000u, 25165824u, 0x0Fu));
    assert(ra_extra_block_valid(0x0A000000u, 0x01800000u, 0x0B600000u, RA_EXTRA_RESERVE_BYTES));
    assert(ra_extra_block_valid(0x0A000000u, 0x01800000u, 0x8B600000u, RA_EXTRA_RESERVE_BYTES));
    assert(ra_extra_block_valid(0x8A000000u, 0x01800000u, 0x0B7FFFFFu, 1));
    assert(!ra_extra_block_valid(0x0A000000u, 0x01800000u, 0x0B7FFFFFu, 2));
    assert(!ra_extra_block_valid(0x08800000u, 0x01800000u, 0x08804000u, RA_EXTRA_RESERVE_BYTES));
    assert(!ra_extra_block_valid(0x0A000000u, 0x01800000u, 0x0C000000u, RA_EXTRA_RESERVE_BYTES));
    assert(!ra_extra_block_valid(0x0A000000u, 0x01800000u, 0x0A000000u, UINT32_MAX));
    assert(!ra_extra_block_valid(0x0A000000u, UINT32_MAX, 0x0A000000u, 1));
    assert(!ra_extra_block_valid(0x0A000000u, 0, 0x0A000000u, 1));
    assert(!ra_extra_block_valid(0x0A000000u, 0x01800000u, 0xAA000000u, 1));
    assert(!ra_extra_block_valid(0x0A000000u, 0x01800000u, 0, 1));
    assert(!ra_extra_block_valid(0x0A000000u, 0x01800000u, 0x09FFFFFFu, 1));
    assert(ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, 0, 128));
    assert(ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, RA_EXTRA_RESERVE_BYTES - 128, 128));
    assert(!ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, RA_EXTRA_RESERVE_BYTES - 128, 129));
    assert(!ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, UINT32_MAX, 128));
    assert(!ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, 1, UINT32_MAX));
    assert(!ra_extra_slice_valid(RA_EXTRA_RESERVE_BYTES, 0, 0));
    assert(ra_extra_user_segment_valid(0x0A000000u, 0x01800000u, 0x0B600000u, 4096));
    assert(!ra_extra_user_segment_valid(0x0A000000u, 0x01800000u, 0x8B600000u, 4096));
    assert(!ra_extra_user_segment_valid(0x0A000000u, 0x01800000u, 0x08804000u, 4096));
    assert(!ra_extra_user_segment_valid(0x0A000000u, 0x01800000u, 0x0B7FFFFFu, 2));
    puts("PASS extra RAM: incompatible user partition rejected, no partition 2/alias fallback, owned slices bounded");
    return 0;
}
