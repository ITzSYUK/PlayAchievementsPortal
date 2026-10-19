/* Host-only heap measurements of our full rc_client session, not a PSP budget.
 * Inspired by Bakkerrrs' arena/churn experiments (see ONLINE_AUDIT.md).
 * No upstream implementation is copied. Only synthetic login and local replies.
 */
#include "ra_client_session.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GNU ld wrapping covers our adapter and the pinned static rcheevos library.
 * Requested bytes exclude these headers, allocator overhead and libc internals.
 * Never use this allocator in the PSP client or redirect a dependency's sources.
 */
typedef union {
    struct { size_t size; } info;
    long double alignment;
    void* pointer_alignment;
    uint64_t integer_alignment;
} allocation;
static size_t live_bytes, peak_bytes, allocated_bytes, freed_bytes, allocations;
void* __real_malloc(size_t);
void* __real_realloc(void*, size_t);
void __real_free(void*);
static void acquired(size_t size)
{
    live_bytes += size;
    allocated_bytes += size;
    ++allocations;
    if (live_bytes > peak_bytes) peak_bytes = live_bytes;
}
void* __wrap_malloc(size_t size)
{
    allocation* p;
    if (size > SIZE_MAX - sizeof(*p)) return NULL;
    p = __real_malloc(sizeof(*p) + size);
    if (!p) return NULL;
    p->info.size = size;
    acquired(size);
    return p + 1;
}
void* __wrap_calloc(size_t count, size_t size)
{
    void* p;
    if (size && count > SIZE_MAX / size) return NULL;
    size *= count;
    p = __wrap_malloc(size);
    if (p) memset(p, 0, size);
    return p;
}
char* __wrap_strdup(const char* source)
{
    const size_t size = strlen(source) + 1;
    char* copy = __wrap_malloc(size);
    if (copy) memcpy(copy, source, size);
    return copy;
}
void __wrap_free(void* bytes)
{
    allocation* p;
    if (!bytes) return;
    p = (allocation*)bytes - 1;
    assert(live_bytes >= p->info.size);
    live_bytes -= p->info.size;
    freed_bytes += p->info.size;
    __real_free(p);
}
void* __wrap_realloc(void* bytes, size_t size)
{
    allocation* p;
    size_t old_size;
    if (!bytes) return __wrap_malloc(size);
    if (!size) { __wrap_free(bytes); return NULL; }
    if (size > SIZE_MAX - sizeof(*p)) return NULL;
    p = (allocation*)bytes - 1;
    old_size = p->info.size;
    p = __real_realloc(p, sizeof(*p) + size);
    if (!p) return NULL; /* The old allocation is still live. */
    live_bytes -= old_size;
    freed_bytes += old_size;
    p->info.size = size;
    acquired(size);
    return p + 1;
}

static uint64_t clock_ms;
static uint64_t now_ms(void* unused) { (void)unused; return clock_ms; }
static uint32_t read_memory(uint32_t address, uint8_t* bytes, uint32_t size, void* unused)
{
    (void)unused;
    if (address < 0x00800000u || address >= 0x02000000u || size > 0x02000000u - address)
        return 0;
    memset(bytes, 0, size);
    return size;
}
/* Fixtures, I/O buffers and printf's internal allocations are outside the heap
 * measurement. We print no input strings, account data, definitions or paths.
 */
static char payload[RA_SESSION_RESPONSE_LIMIT + 1u];
static const char login[] = "{\"Success\":true,\"User\":\"fixture\",\"Token\":\"synthetic-test-only\"}";
static size_t synthetic_set(unsigned count, int distinct, int presence)
{
    size_t used;
    unsigned i;
    int n = snprintf(payload, sizeof(payload),
        "{\"Success\":true,\"GameId\":1,\"Title\":\"Synthetic\",\"ConsoleId\":41,"
        "\"ImageIconUrl\":\"\",%s\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,"
        "\"Title\":\"\",\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Leaderboards\":[],\"Achievements\":[",
        presence ? "\"RichPresencePatch\":\"Lookup:State\\n0x00=Zero\\n0x0b=Eleven\\n\\nDisplay:\\n@State(0xH00800000)\"," : "");
    assert(n > 0 && (size_t)n < sizeof(payload));
    used = (size_t)n;
    for (i = 0; i < count; ++i) {
        n = snprintf(payload + used, sizeof(payload) - used,
            "%s{\"ID\":%u,\"Title\":\"Synthetic %u\",\"Description\":\"Local fixture\","
            "\"Flags\":3,\"Points\":1,\"MemAddr\":\"0xH%08x=1_d0xH%08x=0\","
            "\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}",
            i ? "," : "", i + 1, i + 1, 0x00800000u + (distinct ? i : 0),
            0x00800000u + (distinct ? i : 0));
        assert(n > 0 && (size_t)n < sizeof(payload) - used);
        used += (size_t)n;
    }
    assert(sizeof(payload) - used > 6);
    memcpy(payload + used, "]}]}", 5);
    return used + 4;
}
static void reply(ra_client_session* s, unsigned kind, const char* body, size_t size)
{
    ra_session_request request;
    assert(ra_session_take_request(s, &request) && request.kind == kind);
    assert(ra_session_complete(s, request.ticket, 200, body, size));
}
static int measure(const char* name, size_t size, unsigned expected)
{
    const ra_session_config config = {NULL, now_ms, read_memory, NULL, NULL};
    ra_session_stats stats;
    ra_client_session* s;
    size_t client_bytes, queued_bytes, parsed_bytes, parse_peak, parse_allocated, parse_freed;
    size_t start_allocated, start_freed, frame_allocations;
    unsigned i;
    assert(!live_bytes);
    s = ra_session_create(&config);
    assert(s);
    client_bytes = live_bytes;
    assert(ra_session_login_token(s, "fixture", "synthetic-test-only"));
    reply(s, RA_REQUEST_LOGIN, login, sizeof(login) - 1);
    ra_session_tick(s, 0, 1);
    assert(ra_session_load_game(s, "0123456789abcdef0123456789abcdef"));
    reply(s, RA_REQUEST_SET, payload, size);
    queued_bytes = live_bytes;
    peak_bytes = live_bytes;
    start_allocated = allocated_bytes;
    start_freed = freed_bytes;
    ra_session_tick(s, 0, 1);
    parsed_bytes = live_bytes;
    parse_peak = peak_bytes;
    parse_allocated = allocated_bytes - start_allocated;
    parse_freed = freed_bytes - start_freed;
    ra_session_get_stats(s, &stats);
    if (stats.last_result != RC_OK || !stats.game_loaded || stats.console_id != 41) {
        assert(ra_session_close(s) && !live_bytes);
        fputs("Local response did not load a PSP achievement set\n", stderr);
        return 0;
    }
    assert(stats.spectator && !stats.hardcore && !stats.pending && !stats.buffer_bytes);
    assert(!expected || (stats.achievements == expected && !stats.unsupported));
    /* Steady synthetic frames must not allocate. File inputs can contain rich
     * presence/conditions that produce events, so only idle is measured there. */
    frame_allocations = allocations;
    for (i = 0; i < 120; ++i) { clock_ms += 17; ra_session_tick(s, expected != 0, !expected); }
    frame_allocations = allocations - frame_allocations;
    assert(!frame_allocations);
    assert(ra_session_close(s) && !live_bytes);
    printf("%s,%u,%u,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", name, stats.achievements,
        stats.unsupported, size, client_bytes, queued_bytes, parsed_bytes,
        parse_peak, parse_allocated, parse_freed, frame_allocations, live_bytes);
    return 1;
}
int main(int argc, char** argv)
{
    unsigned pass;
    printf("host_pointer_bits=%zu; requested heap bytes only; excludes TLS/PSP/stack/static/allocator overhead\n", sizeof(void*) * 8);
    puts("case,achievements,unsupported,input_bytes,created_live,queued_live,parsed_live,parse_peak,parse_allocated,parse_freed,steady_allocations,closed_live");
    if (argc == 3 && !strcmp(argv[1], "--sets")) {
        FILE* file = fopen(argv[2], "rb");
        size_t size;
        int extra, error;
        if (!file) { fputs("Cannot open local achievementsets response\n", stderr); return 1; }
        size = fread(payload, 1, sizeof(payload) - 1, file);
        extra = fgetc(file);
        error = ferror(file);
        if (fclose(file) || error || extra != EOF || !size) {
            fputs("Empty, unreadable or oversized local response\n", stderr); return 1;
        }
        payload[size] = '\0';
        if (!measure("local-sets", size, 0)) return 1;
    } else if (argc == 1) {
        /* Repeat with a clean client to detect retained allocations. */
        for (pass = 0; pass < 2; ++pass) {
            assert(measure("one", synthetic_set(1, 0, 0), 1));
            assert(measure("101-shared", synthetic_set(101, 0, 0), 101));
            assert(measure("101-distinct", synthetic_set(101, 1, 0), 101));
            assert(measure("101-shared-rp", synthetic_set(101, 0, 1), 101));
            assert(measure("256-distinct-rp", synthetic_set(256, 1, 1), 256));
        }
    } else {
        fputs("Usage: client_memory_bench [--sets local-achievementsets.json]\n", stderr);
        return 1;
    }
    puts("PASS full rc_client heap: parse peaks/churn, allocation-free steady loop, zero live bytes after close; no network");
    return 0;
}
