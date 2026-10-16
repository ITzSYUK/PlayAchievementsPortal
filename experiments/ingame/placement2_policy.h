#ifndef RA_PLACEMENT2_POLICY_H
#define RA_PLACEMENT2_POLICY_H
#include <stdint.h>
/* Conservative experiment guards, not a measured game memory budget. */
#define RA_PLACEMENT2_MODULE_BUDGET UINT32_C(32768)
#define RA_PLACEMENT2_FREE_FLOOR UINT32_C(524288)
#define RA_PLACEMENT2_MEMORY_GUARD 3
int ra_placement2_partition_valid(uint32_t address, uint32_t size, uint32_t attributes);
int ra_placement2_segment_valid(uint32_t partition_address, uint32_t partition_size,
    uint32_t address, uint32_t size);
int ra_placement2_can_load(uint32_t partition_size, uint32_t total, uint32_t largest);
int ra_placement2_can_hold(uint32_t partition_size, uint32_t total, uint32_t largest);
int ra_placement2_module_sizes_valid(uint32_t text, uint32_t data, uint32_t bss);
#endif
