#ifndef RA_INGAME_SHARED_H
#define RA_INGAME_SHARED_H
#include <stdint.h>
#define RA_INGAME_MAGIC 0x52414947u
#define RA_INGAME_DIR "ms0:/seplugins/RAInGameProbe/"
#define RA_INGAME_UMD "UCUS-98662"
#define RA_INGAME_MODULE "LocoRoco"
#define RA_INGAME_EXPECTED_HASH "199515744d49ef3620a5f9297c6116f9"
#define RA_INGAME_LOG_LIMIT 131072u
#define RA_INGAME_CANCEL (-34001)
typedef struct {
    uint32_t magic, version, size;
    volatile uint32_t stop, suspended;
    volatile uint32_t ready, worker_done, tls_request, tls_done;
    volatile uint32_t network_present;
    volatile int worker_result, tls_result, http_status;
} ra_ingame_shared;
typedef struct { ra_ingame_shared* shared; } ra_ingame_args;
#endif
