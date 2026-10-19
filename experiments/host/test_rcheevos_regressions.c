/* Integration hazards suggested by Bakkerrrs' PSP experiments; original tests
 * against our pinned, unmodified rcheevos and host libc. No kernel libc shim.
 */
#include "rc_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned current_value, reads, triggered[3];
static uint32_t RC_CCONV peek(uint32_t address, uint32_t size, void* unused)
{
    (void)unused;
    /* A wrong binary-prefix interpretation of the hex address fails here. */
    assert(address == 0x0b1234 && size == 1);
    ++reads;
    return current_value;
}
static void RC_CCONV event(const rc_runtime_event_t* e)
{
    if (e->type == RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED) {
        assert(e->id == 1 || e->id == 2);
        ++triggered[e->id];
    }
}
static void shared_history(void)
{
    rc_runtime_t runtime;
    rc_runtime_init(&runtime);
    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0b1234=1_d0xH0b1234=0", NULL, 0) == RC_OK);
    assert(rc_runtime_activate_achievement(&runtime, 2, "0xH0b1234=1_d0xH0b1234=0", NULL, 0) == RC_OK);
    current_value = 0;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    reads = 0;
    current_value = 1;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    /* Both triggers must see the same previous value. A separate memref update
     * per trigger would erase Delta history and suppress the second event. */
    assert(reads == 1 && triggered[1] == 1 && triggered[2] == 1);
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    assert(triggered[1] == 1 && triggered[2] == 1);
    rc_runtime_destroy(&runtime);
}
static void presence_hex_lookup(void)
{
    static const char script[] =
        "Lookup:State\n0x00=Zero\n0x0b=Eleven\n\nDisplay:\n@State(0xH0b1234)\n";
    char buffer[32];
    rc_runtime_t runtime;
    rc_runtime_init(&runtime);
    assert(rc_runtime_activate_richpresence(&runtime, script, NULL, 0) == RC_OK);
    current_value = 11;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    assert(rc_runtime_get_richpresence(&runtime, buffer, sizeof(buffer), peek, NULL, NULL) > 0);
    assert(!strcmp(buffer, "Eleven"));
    current_value = 0;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    assert(rc_runtime_get_richpresence(&runtime, buffer, sizeof(buffer), peek, NULL, NULL) > 0);
    assert(!strcmp(buffer, "Zero"));
    rc_runtime_destroy(&runtime);
}
static void true_at_start(void)
{
    rc_runtime_t runtime;
    memset(triggered, 0, sizeof(triggered));
    rc_runtime_init(&runtime);
    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0b1234=1", NULL, 0) == RC_OK);
    current_value = 1;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    assert(!triggered[1]); /* Must observe false before the first unlock. */
    current_value = 0;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    current_value = 1;
    rc_runtime_do_frame(&runtime, event, peek, NULL, NULL);
    assert(triggered[1] == 1);
    rc_runtime_destroy(&runtime);
}
int main(void)
{
    shared_history();
    presence_hex_lookup();
    true_at_start();
    puts("PASS pinned parser/runtime: hex 0b address and RP lookup, shared Delta, no repeat event, true-at-start waits for false");
    return 0;
}
