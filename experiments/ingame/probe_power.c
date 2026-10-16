#include "probe_power.h"
void ra_probe_power_init(ra_probe_power* state)
{
    state->callback = state->slot = -1;
}
int ra_probe_power_register(ra_probe_power* state, const ra_probe_power_api* api)
{
    if (state->callback >= 0 || state->slot >= 0) return -1;
    state->callback = api->create(api->context);
    if (state->callback < 0) return state->callback;
    state->slot = api->register_callback(-1, state->callback, api->context);
    return state->slot < 0 ? state->slot : 0;
}
int ra_probe_power_release(ra_probe_power* state, const ra_probe_power_api* api)
{
    int rc;
    if (state->slot >= 0) {
        rc = api->unregister_callback(state->slot, api->context);
        if (rc < 0) return rc; /* A registered callback may still run. */
        state->slot = -1;
    }
    if (state->callback >= 0) {
        rc = api->delete_callback(state->callback, api->context);
        if (rc < 0) return rc;
        state->callback = -1;
    }
    return 0;
}
