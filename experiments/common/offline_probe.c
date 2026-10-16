#include "offline_probe.h"
#include "ra_psp_memory.h"
#include "rc_client.h"
#include "rc_consoles.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    printf("FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

typedef struct pending_request {
    rc_client_server_callback_t callback;
    void* data;
    unsigned calls;
    unsigned completions;
    int result;
} pending_request;

static uint32_t platform_reader(uint32_t address, uint8_t* buffer, uint32_t size, void* userdata)
{
    unsigned* calls = (unsigned*)userdata;
    ++*calls;
    if (address == UINT32_C(0x08804000) && size == 4) {
        buffer[0] = 0x12;
        buffer[1] = 0x34;
        buffer[2] = 0x56;
        buffer[3] = 0x78;
        return 4;
    }
    return 0;
}

static uint32_t short_reader(uint32_t address, uint8_t* buffer, uint32_t size, void* userdata)
{
    (void)address; (void)buffer; (void)userdata;
    return size - 1;
}

static uint32_t RC_CCONV client_read(uint32_t address, uint8_t* buffer, uint32_t size, rc_client_t* client)
{
    (void)address; (void)buffer; (void)size; (void)client;
    return 0;
}

static void RC_CCONV offline_server(const rc_api_request_t* request,
    rc_client_server_callback_t callback, void* data, rc_client_t* client)
{
    pending_request* pending = (pending_request*)rc_client_get_userdata(client);
    /* Inspect neither credentials nor request text. No socket is opened. */
    (void)request;
    pending->callback = callback;
    pending->data = data;
    ++pending->calls;
}

static void RC_CCONV login_complete(int result, const char* error,
    rc_client_t* client, void* userdata)
{
    pending_request* pending = (pending_request*)userdata;
    (void)error; (void)client;
    ++pending->completions;
    pending->result = result;
}

static void complete_network_failure(pending_request* pending)
{
    const rc_api_server_response_t response = { "", 0, RC_API_SERVER_RESPONSE_CLIENT_ERROR };
    rc_client_server_callback_t callback = pending->callback;
    void* data = pending->data;
    pending->callback = NULL;
    pending->data = NULL;
    callback(&response, data);
}

static int memory_probe(void)
{
    const rc_memory_regions_t* regions = rc_console_memory_regions(RC_CONSOLE_PSP);
    uint32_t address = 0;
    uint32_t i;
    unsigned calls = 0;
    uint8_t buffer[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
    CHECK(regions->num_regions == 3);
    /* Compare independently with every boundary in the official console map. */
    for (i = 0; i < regions->num_regions; ++i) {
        const rc_memory_region_t* region = &regions->region[i];
        CHECK(ra_psp_memory_translate(region->start_address, 1, RA_PSP_RAM_64_MB, &address));
        CHECK(address == region->real_address);
        CHECK(ra_psp_memory_translate(region->end_address, 1, RA_PSP_RAM_64_MB, &address));
        CHECK(address == region->real_address + region->end_address - region->start_address);
    }
    CHECK(ra_psp_memory_translate(0x01FFFFFF, 1, RA_PSP_RAM_32_MB, &address));
    CHECK(!ra_psp_memory_translate(0x01FFFFFF, 2, RA_PSP_RAM_32_MB, &address));
    CHECK(address == 0);
    CHECK(!ra_psp_memory_translate(0x02000000, 1, RA_PSP_RAM_32_MB, &address));
    CHECK(!ra_psp_memory_translate(0x03FFFFFF, 2, RA_PSP_RAM_64_MB, &address));
    CHECK(!ra_psp_memory_translate(UINT32_MAX, 4, RA_PSP_RAM_64_MB, &address));
    CHECK(!ra_psp_memory_translate(1, UINT32_MAX, RA_PSP_RAM_64_MB, &address));
    CHECK(!ra_psp_memory_translate(0, 0, RA_PSP_RAM_32_MB, &address));
    CHECK(!ra_psp_memory_translate(0, 1, UINT32_MAX, &address));
    CHECK(!ra_psp_memory_translate(0, 1, RA_PSP_RAM_32_MB, NULL));
    CHECK(ra_psp_memory_read(0x02000000, buffer, 1, RA_PSP_RAM_32_MB, platform_reader, &calls) == 0);
    CHECK(calls == 0 && buffer[0] == 0xAA);
    CHECK(ra_psp_memory_read(0x00804000, buffer, 4, RA_PSP_RAM_32_MB, platform_reader, &calls) == 4);
    CHECK(calls == 1 && buffer[0] == 0x12 && buffer[3] == 0x78);
    CHECK(ra_psp_memory_read(0, buffer, 4, RA_PSP_RAM_32_MB, platform_reader, &calls) == 0);
    CHECK(buffer[0] == 0x12);
    CHECK(ra_psp_memory_read(0, buffer, 4, RA_PSP_RAM_32_MB, short_reader, NULL) == 0);
    CHECK(ra_psp_memory_read(0, NULL, 4, RA_PSP_RAM_32_MB, platform_reader, &calls) == 0);
    CHECK(ra_psp_memory_read(0, buffer, 4, RA_PSP_RAM_32_MB, NULL, NULL) == 0);
    puts("PASS official PSP memory map and rejected reads");
    return 0;
}

static int client_probe(void)
{
    pending_request pending;
    rc_client_t* client;
    rc_client_async_handle_t* handle;
    unsigned scenario;
    for (scenario = 0; scenario < 3; ++scenario) {
        memset(&pending, 0, sizeof(pending));
        client = rc_client_create(client_read, offline_server);
        CHECK(client != NULL);
        rc_client_set_userdata(client, &pending);
        rc_client_set_hardcore_enabled(client, 0);
        rc_client_set_spectator_mode_enabled(client, 1);
        CHECK(!rc_client_get_hardcore_enabled(client));
        CHECK(rc_client_get_spectator_mode_enabled(client));
        rc_client_do_frame(client);
        rc_client_idle(client);
        CHECK(pending.calls == 0);
        /* These are synthetic literals, never real account credentials. */
        handle = rc_client_begin_login_with_token(client, "offline-test", "synthetic-test-only",
            login_complete, &pending);
        CHECK(handle != NULL && pending.calls == 1 && pending.callback != NULL);
        if (scenario == 1)
            rc_client_abort_async(client, handle);
        if (scenario == 2)
            rc_client_destroy(client);
        /* v12.5.0 retains the callback data until transport completion, including
         * after abort/destroy. The platform must deliver completion exactly once. */
        complete_network_failure(&pending);
        CHECK(pending.completions == (scenario == 0 ? 1U : 0U));
        if (scenario == 0)
            CHECK(pending.result != RC_OK && rc_client_get_user_info(client) == NULL);
        if (scenario != 2)
            rc_client_destroy(client);
    }
    puts("PASS real rc_client create/idle, softcore/spectator, failure/abort/destroy callbacks");
    return 0;
}

int ra_offline_probe_run(void)
{
    if (memory_probe() != 0 || client_probe() != 0)
        return 1;
    puts("OFFLINE PROBE PASSED (no network, no game memory, no unlock)");
    return 0;
}
