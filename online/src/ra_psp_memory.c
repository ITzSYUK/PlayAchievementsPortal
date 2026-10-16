#include "ra_psp_memory.h"

#include <stddef.h>

int ra_psp_memory_translate(uint32_t address, uint32_t size,
    uint32_t available_ram_bytes, uint32_t* psp_address)
{
    if (psp_address == NULL)
        return 0;
    *psp_address = 0;

    if ((available_ram_bytes != RA_PSP_RAM_32_MB &&
         available_ram_bytes != RA_PSP_RAM_64_MB) || size == 0 ||
        address >= available_ram_bytes || size > available_ram_bytes - address)
        return 0;

    /* rcheevos v12.5.0 PSP regions: kernel, user, extended RAM.
     * They are contiguous and share the same physical address displacement. */
    *psp_address = UINT32_C(0x08000000) + address;
    return 1;
}

uint32_t ra_psp_memory_read(uint32_t address, uint8_t* buffer, uint32_t size,
    uint32_t available_ram_bytes, ra_psp_platform_read_t reader, void* userdata)
{
    uint32_t psp_address;
    if (buffer == NULL || reader == NULL ||
        !ra_psp_memory_translate(address, size, available_ram_bytes, &psp_address))
        return 0;

    /* rc_client interprets 0 as invalid. Never fill unread data with zeroes. */
    return reader(psp_address, buffer, size, userdata) == size ? size : 0;
}
