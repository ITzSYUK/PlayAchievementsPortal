#include <pspkernel.h>
#include <pspsdk.h>
#include <stdint.h>
#include <string.h>
#include "entropy_request.h"

PSP_MODULE_INFO("RAProbeEntropy", 0x1000, 0, 1);
int sceUtilsBufferCopyWithRange(void*, int, void*, int, int);
static void erase(void* p, unsigned length)
{
    volatile unsigned char* bytes = p;
    while (length--) *bytes++ = 0;
}
int module_start(SceSize args, void* argp)
{
    ra_entropy_request request;
    ra_entropy_output* out;
    unsigned char block[32] __attribute__((aligned(64)));
    unsigned char seed[RA_ENTROPY_BYTES];
    unsigned offset;
    int rc = -1;
    uintptr_t address;
    if (args != sizeof(request) || !argp) return -1;
    memcpy(&request, argp, sizeof(request));
    out = request.output; address = (uintptr_t)out;
    /* Probe requests MEMSIZE=0: restrict to base user partition, never kernel
     * RAM, even on the 64 MiB PSP-3000. Caller owns this live allocation. */
    if (address < 0x08800000u || address > 0x0A000000u - sizeof(*out) ||
        address % sizeof(int)) return -1;
    memset(seed, 0, sizeof(seed));
    memset(out, 0, sizeof(*out)); out->status = -1;
    for (offset = 0; offset < sizeof(seed); offset += 20) {
        memset(block, 0, sizeof(block));
        rc = sceUtilsBufferCopyWithRange(block, 20, NULL, 0, 14);
        if (rc != 0) break;
        /* Basic stuck-output detection, not a proof of entropy strength. */
        {
            unsigned i, nonzero = 0;
            for (i = 0; i < 20; ++i) nonzero |= block[i];
            if (!nonzero || (offset && memcmp(block, seed + offset - 20, 20) == 0)) { rc = -2; break; }
        }
        memcpy(seed + offset, block, 20);
    }
    if (rc == 0) memcpy(out->bytes, seed, sizeof(seed));
    out->status = rc;
    erase(block, sizeof(block)); erase(seed, sizeof(seed));
    sceKernelDcacheWritebackInvalidateRange(out, sizeof(*out));
    /* Successful start allows explicit stop/unload, even if KIRK failed. */
    return 0;
}
int module_stop(SceSize args, void* argp)
{ (void)args; (void)argp; return 0; }
