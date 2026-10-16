#include "ra_client_session.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Synthetic payloads only. Linker wrapping injects a failed response-copy
 * allocation and checks that every watched copy is erased before free. */
void* __real_malloc(size_t);
void __real_free(void*);
static int fail_next_malloc;
static size_t watch_size, watched_bytes;
static void* watched_body;
static unsigned erased_copies;
void* __wrap_malloc(size_t size)
{
    void* bytes;
    if (fail_next_malloc) { fail_next_malloc = 0; watch_size = 0; return NULL; }
    bytes = __real_malloc(size);
    if (watch_size && size == watch_size) {
        assert(!watched_body && bytes);
        watched_body = bytes; watched_bytes = size; watch_size = 0;
    }
    return bytes;
}
void __wrap_free(void* bytes)
{
    if (bytes && bytes == watched_body) {
        size_t i;
        for (i = 0; i < watched_bytes; ++i) assert(((unsigned char*)bytes)[i] == 0);
        watched_body = NULL; watched_bytes = 0; ++erased_copies;
    }
    __real_free(bytes);
}
static uint64_t clock_ms;
static uint64_t now_ms(void* unused) { (void)unused; return clock_ms; }
static const ra_session_config config = { NULL, now_ms, NULL, NULL, NULL };
static const char login[] = "{\"Success\":true,\"User\":\"fixture\",\"Token\":\"synthetic-test-only\"}";
static const char game[] =
    "{\"Success\":true,\"GameId\":1,\"Title\":\"Synthetic test\",\"ConsoleId\":41,"
    "\"ImageIconUrl\":\"\",\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":\"\","
    "\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Leaderboards\":[],\"Achievements\":[{"
    "\"ID\":1,\"Title\":\"Synthetic\",\"Description\":\"Local test\",\"Flags\":3,\"Points\":1,"
    "\"MemAddr\":\"0xH000000=1\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}]}]}";
static char* payload(size_t size)
{
    const char padding[] = ",\"Padding\":\"";
    size_t prefix = sizeof(game)-2, position = prefix + sizeof(padding)-1;
    char* bytes = malloc(size + 1);
    assert(bytes && size > position + 2);
    memcpy(bytes, game, prefix); memcpy(bytes + prefix, padding, sizeof(padding)-1);
    memset(bytes + position, 'x', size - position - 2);
    bytes[size-2] = '"'; bytes[size-1] = '}'; bytes[size] = 0;
    return bytes;
}
static ra_client_session* begin_set(ra_session_request* request)
{
    ra_session_stats stats;
    ra_client_session* session = ra_session_create(&config);
    assert(session && ra_session_login_token(session, "fixture", "synthetic-test-only"));
    assert(ra_session_take_request(session, request));
    assert(ra_session_complete(session, request->ticket, 200, login, sizeof(login)-1));
    ra_session_tick(session, 0, 1); ra_session_get_stats(session, &stats);
    assert(stats.logged_in && stats.last_result == RC_OK);
    assert(ra_session_load_game(session, "0123456789abcdef0123456789abcdef"));
    assert(ra_session_take_request(session, request));
    assert(!strncmp(request->post_data, "r=achievementsets&", 18));
    return session;
}
static void copy_and_parse(size_t size)
{
    ra_session_request request;
    ra_session_stats stats;
    ra_client_session* session = begin_set(&request);
    char* bytes = payload(size);
    size_t borrowed_post = request.post_size + 1;
    watch_size = size + 1;
    assert(ra_session_complete(session, request.ticket, 200, bytes, size));
    assert(watched_body && !watch_size);
    ra_session_get_stats(session, &stats);
    assert(stats.last_copy_error == RA_SESSION_COPY_OK && stats.pending == 1 && !stats.game_loaded);
    assert(stats.buffer_bytes == borrowed_post + size + 1 && stats.peak_buffer_bytes == stats.buffer_bytes);
    assert(stats.peak_buffer_bytes <= RA_SESSION_BUFFER_LIMIT);
    /* The caller can overwrite/release the transport buffer after complete. */
    memset(bytes, '?', size); free(bytes);
    ra_session_tick(session, 0, 1); ra_session_get_stats(session, &stats);
    assert(!watched_body && stats.last_result == RC_OK && stats.game_loaded && stats.game_id == 1);
    assert(stats.achievements == 1 && stats.unsupported == 1 && stats.console_id == 41);
    assert(stats.buffer_bytes == 0 && !stats.pending && !stats.frame_calls && !stats.earned_events);
    assert(ra_session_close(session));
}
int main(void)
{
    ra_session_request request;
    ra_session_stats stats;
    ra_client_session* session;
    char* bytes;
    copy_and_parse(129891);
    copy_and_parse(RA_SESSION_RESPONSE_LIMIT);
    session = begin_set(&request); bytes = payload(129891);
    watch_size = 129892;
    assert(ra_session_complete(session, request.ticket, 200, bytes, 129891));
    free(bytes); assert(watched_body && ra_session_close(session) && !watched_body);
    session = begin_set(&request);
    /* No read of the one-byte pointer is allowed when size exceeds the cap. */
    assert(ra_session_complete(session, request.ticket, 200, "x", RA_SESSION_RESPONSE_LIMIT + 1));
    ra_session_get_stats(session, &stats); assert(stats.last_copy_error == RA_SESSION_COPY_BODY_LIMIT);
    ra_session_tick(session, 0, 1); ra_session_get_stats(session, &stats);
    assert(!stats.game_loaded && stats.last_result != RC_OK && !stats.buffer_bytes && !stats.pending);
    assert(ra_session_close(session));
    session = begin_set(&request); bytes = payload(129891);
    watch_size = 129892; fail_next_malloc = 1;
    assert(ra_session_complete(session, request.ticket, 200, bytes, 129891));
    free(bytes); assert(!fail_next_malloc && !watched_body && !watch_size);
    ra_session_get_stats(session, &stats); assert(stats.last_copy_error == RA_SESSION_COPY_NO_MEMORY);
    ra_session_tick(session, 0, 1); ra_session_get_stats(session, &stats);
    assert(!stats.game_loaded && stats.last_result != RC_OK && !stats.buffer_bytes && !stats.pending);
    assert(ra_session_close(session));
    session = begin_set(&request);
    clock_ms += RA_SESSION_TIMEOUT_MS + 1;
    assert(ra_session_complete(session, request.ticket, 200, "x", 1));
    ra_session_get_stats(session, &stats); assert(stats.last_copy_error == RA_SESSION_COPY_TRANSPORT);
    ra_session_tick(session,0,1);ra_session_get_stats(session,&stats);
    assert(stats.last_http_status==RA_SESSION_RETRYABLE && !stats.pending && !stats.game_loaded);
    assert(ra_session_close(session));
    assert(erased_copies == 3);
    puts("PASS large session: real rc_client parses synthetic 129891/262144-byte sets, queue cap/peak, copy isolation, erase-before-free, close/limit/timeout/OOM");
    return 0;
}
