#include "probe_power.h"
#include <assert.h>
#include <stdio.h>
typedef struct {
    int create_result, register_result, unregister_result, delete_result;
    int creates, registrations, unregistrations, deletions;
} firmware;
static int create(void* context)
{ firmware* f = context; ++f->creates; return f->create_result; }
static int register_callback(int slot, int callback, void* context)
{
    firmware* f = context;
    assert(slot == -1 && callback == 73);
    ++f->registrations;
    return f->register_result;
}
static int unregister_callback(int slot, void* context)
{
    firmware* f = context;
    assert(slot == f->register_result && slot >= 0 && f->deletions == 0);
    ++f->unregistrations;
    return f->unregister_result;
}
static int delete_callback(int callback, void* context)
{
    firmware* f = context;
    assert(callback == 73);
    assert(f->register_result < 0 || f->unregistrations > 0);
    ++f->deletions;
    return f->delete_result;
}
static void exercise(int create_rc, int register_rc, int unregister_rc, int delete_rc)
{
    firmware f = {create_rc, register_rc, unregister_rc, delete_rc, 0, 0, 0, 0};
    ra_probe_power_api api = {create, register_callback, unregister_callback, delete_callback, &f};
    ra_probe_power state;
    int rc, previous_deletions, retry_delete;
    ra_probe_power_init(&state);
    rc = ra_probe_power_register(&state, &api);
    assert(rc == (create_rc < 0 ? create_rc : register_rc < 0 ? register_rc : 0));
    if (create_rc >= 0) {
        assert(ra_probe_power_register(&state, &api) == -1);
        assert(f.creates == 1); /* Never leak a second callback on retry. */
    }
    rc = ra_probe_power_release(&state, &api);
    if (create_rc < 0) {
        assert(rc == 0 && f.registrations == 0 && f.deletions == 0);
    } else if (register_rc >= 0 && unregister_rc < 0) {
        assert(rc == unregister_rc && state.slot == register_rc && state.callback == 73);
        assert(f.deletions == 0); /* Live callback code must not be unloaded. */
    } else if (delete_rc < 0) {
        assert(rc == delete_rc && state.slot < 0 && state.callback == 73);
    } else {
        assert(rc == 0 && state.slot < 0 && state.callback < 0 && f.deletions == 1);
        assert(ra_probe_power_release(&state, &api) == 0 && f.deletions == 1);
    }
    /* Retry failed cleanup without re-registering or deleting a live callback. */
    previous_deletions = f.deletions;
    retry_delete = state.callback >= 0;
    f.unregister_result = f.delete_result = 0;
    assert(ra_probe_power_release(&state, &api) == 0);
    assert(state.slot < 0 && state.callback < 0);
    assert(f.deletions == previous_deletions + retry_delete);
}
int main(void)
{
    exercise(-2, 4, 0, 0);
    exercise(73, -4, 0, 0);
    exercise(73, 0, 0, 0); /* Slot zero is successful ownership, not failure. */
    exercise(73, 7, 0, 0);
    exercise(73, 7, -5, 0);
    exercise(73, 7, 0, -6);
    exercise(73, -4, 0, -6);
    puts("PASS power callback ownership and failed registration/cleanup");
    return 0;
}
