#ifndef RA_FRAME_ENGINE_H
#define RA_FRAME_ENGINE_H
/* rc_client has one serialized owner. Transport only publishes responses.
 * The game callback never waits for the worker to release ownership. */
static volatile uint32_t engine_gate, engine_enabled, engine_last_us, engine_fault;
static volatile int frame_queue_owner = -1;
static volatile uint32_t engine_duration_max, engine_stack_min = UINT32_MAX;
static void engine_frame(void)
{
    uint32_t started; int stack;
    if (!engine_enabled || control->stop) return;
    if (__sync_lock_test_and_set(&engine_gate, 1)) {
        engine_fault = 1; control->stop = 1; return;
    }
    if (engine_enabled && !control->stop) {
        stack = sceKernelGetThreadStackFreeSize(0);
        if (stack < 16384) { engine_fault = 2; control->stop = 1; }
        else if (sceKernelPollSema(queue_guard, 1) < 0) { engine_fault = 3; control->stop = 1; }
        else {
            frame_queue_owner = sceKernelGetThreadId();
            if ((uint32_t)stack < engine_stack_min) engine_stack_min = (uint32_t)stack;
            started = sceKernelGetSystemTimeLow();
            session_owner_tick();
            ++control->frame_calls;
            engine_last_us = sceKernelGetSystemTimeLow();
            if (engine_last_us - started > engine_duration_max)
                engine_duration_max = engine_last_us - started;
            frame_queue_owner = -1;
            if (sceKernelSignalSema(queue_guard, 1) < 0) { engine_fault = 4; control->stop = 1; }
        }
    }
    __sync_lock_release(&engine_gate);
}
static void engine_stop(void)
{
    /* Closing the gate drains current evaluation. A deferred wrapper can
     * only see disabled state afterwards, even after runtime is destroyed. */
    while (__sync_lock_test_and_set(&engine_gate, 1)) sceKernelDelayThread(1000);
    engine_enabled = 0;
    __sync_synchronize();
    __sync_lock_release(&engine_gate);
}
#endif
