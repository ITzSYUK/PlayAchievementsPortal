#ifndef RA_PROBE_POWER_H
#define RA_PROBE_POWER_H
/* Callback ownership must survive failed registration and failed cleanup.
 * Firmware calls are injected so these failure paths can be checked on host. */
typedef struct { int callback, slot; } ra_probe_power;
typedef struct {
    int (*create)(void*);
    int (*register_callback)(int, int, void*);
    int (*unregister_callback)(int, void*);
    int (*delete_callback)(int, void*);
    void* context;
} ra_probe_power_api;
void ra_probe_power_init(ra_probe_power* state);
int ra_probe_power_register(ra_probe_power* state, const ra_probe_power_api* api);
int ra_probe_power_release(ra_probe_power* state, const ra_probe_power_api* api);
#endif
