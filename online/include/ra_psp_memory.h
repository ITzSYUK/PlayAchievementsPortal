#ifndef RA_PSP_MEMORY_H
#define RA_PSP_MEMORY_H

#include <stdint.h>

#define RA_PSP_RAM_32_MB UINT32_C(0x02000000)
#define RA_PSP_RAM_64_MB UINT32_C(0x04000000)

/* The platform reader must validate actual accessibility before dereferencing.
 * Declaring a 64 MB PSP model alone does not enable the extended game region. */
typedef uint32_t (*ra_psp_platform_read_t)(uint32_t psp_address,
    uint8_t* buffer, uint32_t size, void* userdata);

int ra_psp_memory_translate(uint32_t address, uint32_t size,
    uint32_t available_ram_bytes, uint32_t* psp_address);
uint32_t ra_psp_memory_read(uint32_t address, uint8_t* buffer, uint32_t size,
    uint32_t available_ram_bytes, ra_psp_platform_read_t reader, void* userdata);

#endif
