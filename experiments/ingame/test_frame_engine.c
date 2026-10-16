#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include "client_shared.h"
static ra_client_control storage, *control = &storage;
static int queue_guard = 9;
static void* runtime = (void*)1;
static unsigned ticks, timestamp;
static int stack_free = 32768, poll_error, signal_error, sem_owned;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int hold_tick, tick_entered, stop_started;
static int sceKernelGetThreadStackFreeSize(int id) { assert(id == 0); return stack_free; }
static int sceKernelPollSema(int id, int count)
{ assert(id == 9 && count == 1); if (poll_error) return -1; assert(!sem_owned); sem_owned = 1; return 0; }
static int sceKernelSignalSema(int id, int count)
{ assert(id == 9 && count == 1 && sem_owned); sem_owned = 0; return signal_error ? -1 : 0; }
static int sceKernelGetThreadId(void) { return 7; }
static uint32_t sceKernelGetSystemTimeLow(void) { timestamp += 50; return timestamp; }
static void sceKernelDelayThread(int us) { assert(us == 1000); sched_yield(); }
static void session_owner_tick(void)
{
    assert(runtime && sem_owned);
    ++ticks;
    pthread_mutex_lock(&mutex);
    tick_entered = 1;
    pthread_cond_broadcast(&cond);
    while (hold_tick) pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}
#include "../psp/ingame/https/user/frame_engine.h"
static void reset(void)
{
    memset(&storage, 0, sizeof(storage));
    ticks = timestamp = 0; stack_free = 32768;
    poll_error = signal_error = sem_owned = 0;
    engine_gate = engine_fault = engine_last_us = engine_duration_max = 0;
    engine_stack_min = UINT32_MAX; engine_enabled = 1; frame_queue_owner = -1;
    hold_tick = tick_entered = stop_started = 0;
}
static void* run_frame(void* unused) { (void)unused; engine_frame(); return NULL; }
static void* stop_frame(void* unused)
{
    (void)unused;
    pthread_mutex_lock(&mutex); stop_started = 1; pthread_cond_broadcast(&cond); pthread_mutex_unlock(&mutex);
    engine_stop(); return NULL;
}
int main(void)
{
    pthread_t frame, stop;
    reset(); engine_enabled = 0; engine_frame(); assert(!ticks);
    reset(); control->stop = 1; engine_frame(); assert(!ticks);
    reset(); engine_gate = 1; engine_frame(); assert(engine_fault == 1 && control->stop && !ticks);
    reset(); stack_free = 16383; engine_frame(); assert(engine_fault == 2 && !ticks && !sem_owned && !engine_gate);
    reset(); poll_error = 1; engine_frame(); assert(engine_fault == 3 && !ticks && !engine_gate);
    reset(); signal_error = 1; engine_frame(); assert(engine_fault == 4 && ticks == 1 && !engine_gate);
    reset(); engine_frame(); assert(ticks == 1 && control->frame_calls == 1 && engine_stack_min == 32768);
    assert(engine_duration_max == 50 && frame_queue_owner == -1 && !sem_owned);
    reset(); hold_tick = 1;
    assert(!pthread_create(&frame, NULL, run_frame, NULL));
    pthread_mutex_lock(&mutex); while (!tick_entered) pthread_cond_wait(&cond, &mutex); pthread_mutex_unlock(&mutex);
    assert(!pthread_create(&stop, NULL, stop_frame, NULL));
    pthread_mutex_lock(&mutex); while (!stop_started) pthread_cond_wait(&cond, &mutex);
    assert(engine_enabled && engine_gate); hold_tick = 0; pthread_cond_broadcast(&cond); pthread_mutex_unlock(&mutex);
    assert(!pthread_join(frame, NULL) && !pthread_join(stop, NULL));
    assert(!engine_enabled && !engine_gate && ticks == 1);
    runtime = NULL; engine_frame(); assert(ticks == 1); /* Deferred entry after client destruction. */
    puts("PASS frame engine: nonblocking admission, stack/queue failures, serialized evaluation, shutdown waits for active call, deferred entry cannot access destroyed client");
    return 0;
}
