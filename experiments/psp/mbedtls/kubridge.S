    .set noreorder
#include "pspimport.s"
/* ARK KUBridge interface, also exported by pinned ARK5 SystemControl.
 * Weak import: missing CFW API must result in a load error, never fallback. */
    IMPORT_START "KUBridge",0x40090000
    IMPORT_FUNC "KUBridge",0x4C25EA72,kuKernelLoadModule
