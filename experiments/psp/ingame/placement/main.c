#include <pspkernel.h>
/* Load-only target: the bootstrap MUST NOT call module_start/module_stop.
 * No CRT, heap, threads, callbacks, imports, or constructors are required. */
PSP_MODULE_INFO("RAPlacementUser", 0, 0, 8);
volatile unsigned char placement_data[1024] = {0x52, 0x41, 0x38};
volatile unsigned char placement_zero[1024];
int module_start(SceSize args, void* argp)
{
    (void)args; (void)argp;
    placement_zero[0] = placement_data[0] ^ 0x52u;
    return placement_zero[0];
}
int module_stop(SceSize args, void* argp)
{ (void)args; (void)argp; return 0; }
