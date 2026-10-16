#include "ra_client_session.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint64_t clock_ms;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t owner;
static uint64_t now_ms(void* unused) { (void)unused; return clock_ms; }
static void lock_queue(void* unused) { (void)unused; assert(pthread_mutex_lock(&queue_lock) == 0); }
static void unlock_queue(void* unused) { (void)unused; assert(pthread_mutex_unlock(&queue_lock) == 0); }
static ra_session_config config = { NULL, now_ms, NULL, lock_queue, unlock_queue };
static const char login_response[] = "{\"Success\":true,\"User\":\"fixture\",\"Token\":\"synthetic-test-only\"}";
/* Entirely synthetic set/condition. No captured service data or real triggers. */
static const char game_response[] =
    "{\"Success\":true,\"GameId\":1,\"Title\":\"Synthetic test\",\"ConsoleId\":41,"
    "\"ImageIconUrl\":\"\",\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":\"\","
    "\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Leaderboards\":[],\"Achievements\":[{"
    "\"ID\":1,\"Title\":\"Synthetic\",\"Description\":\"Local test\",\"Flags\":3,\"Points\":1,"
    "\"MemAddr\":\"0xH000000=1\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}]}]}";
static uint8_t game_byte;
static uint32_t read_memory(uint32_t address, uint8_t* bytes, uint32_t size, void* unused)
{
    (void)unused;
    assert(pthread_equal(pthread_self(), owner));
    if (address != 0 || size != 1) return 0;
    *bytes = game_byte; return 1;
}
static void begin_login(ra_client_session* session, ra_session_request* request)
{
    assert(ra_session_login_token(session, "fixture", "synthetic-test-only"));
    assert(ra_session_take_request(session, request));
    assert(strcmp(request->url, "https://retroachievements.org/dorequest.php") == 0);
    assert(request->post_data && request->post_size == strlen(request->post_data));
}
static void* deliver_login(void* data)
{
    ra_client_session* session = data;
    ra_session_request request;
    assert(ra_session_take_request(session, &request));
    assert(ra_session_complete(session, request.ticket, 200, login_response, sizeof(login_response) - 1));
    assert(!ra_session_complete(session, request.ticket, 200, login_response, sizeof(login_response) - 1));
    return NULL;
}
static void* deliver_session(void* data)
{
    ra_client_session* session = data;
    unsigned call;
    const struct timespec delay = { 0, 1000000 };
    for (call = 0; call < 2; ++call) {
        ra_session_request request;
        const char* body;
        unsigned spin;
        for (spin = 0; spin < 5000 && !ra_session_take_request(session, &request); ++spin) nanosleep(&delay, NULL);
        assert(spin < 5000);
        if (call == 0) {
            assert(!strncmp(request.post_data, "r=login2&", 9)); body = login_response;
        } else {
            assert(!strncmp(request.post_data, "r=achievementsets&", 18)); body = game_response;
        }
        assert(ra_session_complete(session, request.ticket, 200, body, strlen(body)));
    }
    return NULL;
}
int main(void)
{
    ra_client_session* session;
    ra_session_request request;
    ra_session_stats stats;
    pthread_t transport;
    unsigned scenario;
    owner = pthread_self();
    assert(!ra_session_create(NULL));
    config.unlock = NULL; assert(!ra_session_create(&config)); config.unlock = unlock_queue;
    session = ra_session_create(&config); assert(session);
    ra_session_tick(session, 0, 0); ra_session_tick(session, 1, 1);
    ra_session_get_stats(session, &stats);
    assert(stats.idle_calls == 2 && stats.frame_calls == 0 && stats.pending == 0);
    assert(stats.spectator && !stats.hardcore);
    assert(!ra_session_load_game(session, "0123456789abcdef0123456789abcdef"));
    assert(ra_session_close(session));
    session = ra_session_create(&config); assert(session);
    begin_login(session, &request);
    {
        const char rejected[] = "{\"Success\":false,\"Error\":\"synthetic secret is never logged\",\"Code\":\"invalid_credentials\"}";
        assert(ra_session_complete(session, request.ticket, 200, rejected, sizeof(rejected) - 1));
        ra_session_tick(session, 0, 1);
        ra_session_get_stats(session, &stats);
        assert(!stats.logged_in && !stats.auth_pending && stats.last_result != RC_OK);
        assert(!ra_session_load_game(session, "0123456789abcdef0123456789abcdef"));
    }
    assert(ra_session_close(session));
    for (scenario = 0; scenario < 5; ++scenario) {
        session = ra_session_create(&config); assert(session);
        begin_login(session, &request);
        assert(!ra_session_login_token(session, "fixture", "synthetic-test-only"));
        if (scenario == 0) {
            /* Close must retain bytes/callbacks while the transport owns them. */
            assert(!ra_session_close(session));
            assert(ra_session_request_cancelled(session, request.ticket));
            assert(strlen(request.post_data) == request.post_size);
            assert(ra_session_complete(session, request.ticket, -1, NULL, 0));
            assert(ra_session_close(session));
            continue;
        }
        if (scenario == 1) assert(ra_session_complete(session, request.ticket, 200, "x", RA_SESSION_RESPONSE_LIMIT + 1));
        else if (scenario == 2) assert(ra_session_complete(session, request.ticket, 200, "malformed", 9));
        else if (scenario == 3) assert(ra_session_complete(session, request.ticket, 200, NULL, 1));
        else assert(ra_session_complete(session, request.ticket, -1, NULL, 0));
        assert(!ra_session_complete(session, request.ticket, -1, NULL, 0));
        ra_session_get_stats(session, &stats); assert(stats.completed == 0 && stats.pending == 1);
        ra_session_tick(session, 0, 1);
        ra_session_get_stats(session, &stats);
        assert(stats.last_result != RC_OK && stats.completed == 1 && stats.pending == 0 && stats.buffer_bytes == 0);
        assert(ra_session_close(session));
    }
    session = ra_session_create(&config); assert(session);
    assert(ra_session_login_token(session, "fixture", "synthetic-test-only"));
    assert(ra_session_close(session)); /* Queued, not borrowed by transport. */
    session = ra_session_create(&config); assert(session);
    begin_login(session, &request);
    assert(ra_session_complete(session, request.ticket, 200, login_response, sizeof(login_response) - 1));
    assert(ra_session_close(session)); /* Completed response cancelled before owner delivery. */
    session = ra_session_create(&config); assert(session);
    assert(ra_session_login_token(session, "fixture", "synthetic-test-only"));
    clock_ms += RA_SESSION_TIMEOUT_MS + 1;
    ra_session_tick(session, 0, 1);
    assert(!ra_session_take_request(session, &request));
    ra_session_get_stats(session, &stats); assert(stats.pending == 0 && stats.last_result != RC_OK);
    assert(ra_session_close(session));
    session = ra_session_create(&config); assert(session);
    assert(ra_session_login_token(session, "fixture", "synthetic-test-only"));
    assert(pthread_create(&transport, NULL, deliver_login, session) == 0);
    assert(pthread_join(transport, NULL) == 0);
    ra_session_get_stats(session, &stats); assert(stats.completed == 0);
    ra_session_tick(session, 0, 1);
    ra_session_get_stats(session, &stats); assert(stats.completed == 1 && stats.last_result == RC_OK);
    assert(ra_session_load_game(session, "0123456789abcdef0123456789abcdef"));
    assert(ra_session_take_request(session, &request));
    assert(ra_session_complete(session, request.ticket, -1, NULL, 0));
    ra_session_tick(session, 0, 1);
    assert(ra_session_close(session));
    session = ra_session_create(&config); assert(session);
    begin_login(session, &request);
    assert(!ra_session_request_cancelled(session, request.ticket));
    clock_ms += RA_SESSION_TIMEOUT_MS + 1;
    assert(ra_session_request_cancelled(session, request.ticket));
    assert(!ra_session_close(session)); /* active expired request still owns data */
    assert(ra_session_complete(session, request.ticket, 200, login_response, sizeof(login_response) - 1));
    assert(ra_session_close(session));
    config.read_memory = read_memory;
#ifdef RA_SESSION_NO_FRAMES
    config.read_memory = NULL; /* Exact PSP policy until its memory adapter is validated. */
#endif
    session = ra_session_create(&config); assert(session);
    begin_login(session, &request);
    assert(ra_session_complete(session, request.ticket, 200, login_response, sizeof(login_response) - 1));
    ra_session_tick(session, 0, 1);
    assert(ra_session_load_game(session, "0123456789abcdef0123456789abcdef"));
    for (scenario = 0; scenario < 8; ++scenario) {
        if (ra_session_take_request(session, &request)) {
            const char* body;
            if (strstr(request.post_data, "r=gameid")) body = "{\"Success\":true,\"GameID\":1}";
            else if (strstr(request.post_data, "r=achievementsets")) body = game_response;
            else if (strstr(request.post_data, "r=startsession")) body = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}";
            else { assert(!"unexpected synthetic request route"); body = ""; }
            assert(ra_session_complete(session, request.ticket, 200, body, strlen(body)));
        }
        ra_session_tick(session, 0, 1);
    }
    ra_session_get_stats(session, &stats);
    assert(stats.game_loaded && stats.last_result == RC_OK && stats.pending == 0 && stats.earned_events == 0);
    assert(stats.game_id == 1 && stats.console_id == 41 && stats.achievements == 1);
#ifdef RA_SESSION_NO_FRAMES
    assert(stats.unsupported == 1);
    clock_ms += 60000; ra_session_tick(session, 0, 1); /* Spectator schedules no session/ping. */
#else
    assert(stats.unsupported == 0);
#endif
    assert(stats.submitted == 2 && stats.completed == 2); /* login2 + set, no startsession or ping */
    for (scenario = 0; scenario < 30; ++scenario) ra_session_tick(session, 1, 0);
    game_byte = 1;
    ra_session_tick(session, 0, 0); ra_session_tick(session, 1, 1);
    ra_session_get_stats(session, &stats); assert(stats.earned_events == 0);
    ra_session_tick(session, 1, 0);
    ra_session_get_stats(session, &stats);
#ifdef RA_SESSION_NO_FRAMES
    assert(stats.earned_events == 0 && stats.frame_calls == 0 && stats.pending == 0);
#else
    assert(stats.earned_events == 1 && stats.pending == 0);
#endif
    assert(!ra_session_take_request(session, &request)); /* spectator never submits award */
    assert(ra_session_close(session));
    /* Owner idle and transport complete run concurrently, just as on PSP. */
    session = ra_session_create(&config); assert(session);
    assert(ra_session_login_token(session, "fixture", "synthetic-test-only"));
    assert(pthread_create(&transport, NULL, deliver_session, session) == 0);
    {
        unsigned spin; int loading = 0;
        const struct timespec delay = { 0, 1000000 };
        for (spin = 0; spin < 5000; ++spin) {
            ra_session_tick(session, 0, 1);
            ra_session_get_stats(session, &stats);
            if (stats.logged_in && !loading) {
                assert(ra_session_load_game(session, "0123456789abcdef0123456789abcdef")); loading = 1;
            }
            if (stats.game_loaded) break;
            nanosleep(&delay, NULL);
        }
        assert(spin < 5000 && stats.game_loaded && stats.pending == 0 && stats.earned_events == 0);
    }
    assert(pthread_join(transport, NULL) == 0);
    assert(ra_session_close(session));
    puts("PASS client session: real rc_client + synthetic spectator set, owner-only completion/memory, login failure, route boundary, caps/cancel/tickets/pause/timeout, safe close");
    return 0;
}
