#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

#include "offline_probe.h"
#include "ra_psp_hash.h"

#include <stdio.h>

PSP_MODULE_INFO("RAOfflineProbe", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);

static int exit_callback(int arg1, int arg2, void* common)
{
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void* argp)
{
    const int callback = sceKernelCreateCallback("Exit callback", exit_callback, NULL);
    (void)args; (void)argp;
    sceKernelRegisterExitCallback(callback);
    sceKernelSleepThreadCB();
    return 0;
}

int main(void)
{
    const int thread = sceKernelCreateThread("Callbacks", callback_thread, 0x11, 0x1000, 0, NULL);
    SceCtrlData buttons;
    char hash[33];
    sceKernelStartThread(thread, 0, NULL);
    pspDebugScreenInit();
    pspDebugScreenPrintf("Offline probe; see stdout.txt for results.\n");
    pspDebugScreenPrintf("This does not read a game's memory or connect to RA.\n");
    if (freopen("stdout.txt", "w", stdout) == NULL) {
        pspDebugScreenPrintf("Cannot create stdout.txt.\n");
    }
    else {
        pspDebugScreenPrintf("Result: %s\n", ra_offline_probe_run() == 0 ? "PASS" : "FAIL");
        /* Optional original files copied by the owner beside EBOOT.PBP.
         * An XMB homebrew does not imply access to the game's disc0. */
        if (ra_psp_hash_disc_files(hash, "PARAM.SFO", "EBOOT.BIN")) {
            printf("Original-file comparison candidate: %s\n", hash);
            pspDebugScreenPrintf("File hash: %s\n", hash);
        }
        else {
            puts("Optional PARAM.SFO/EBOOT.BIN hash unavailable");
        }
        fflush(stdout);
    }
    pspDebugScreenPrintf("Press CROSS to exit.\n");
    do {
        sceCtrlReadBufferPositive(&buttons, 1);
        sceKernelDelayThread(50000);
    } while (!(buttons.Buttons & PSP_CTRL_CROSS));
    sceKernelExitGame();
    return 0;
}
