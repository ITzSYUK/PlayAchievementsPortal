#ifndef RA_CLIENT_SESSION_H
#define RA_CLIENT_SESSION_H
#include "rc_client.h"
#include "ra_response_limits.h"
#define RA_SESSION_SLOTS 2
#define RA_SESSION_POST_LIMIT 8192u
#define RA_SESSION_RESPONSE_LIMIT RA_RESPONSE_BODY_LIMIT
/* One maximum POST and response, including both terminating NUL bytes.
 * Both slots together must fit this shared budget. */
#define RA_SESSION_BUFFER_LIMIT (RA_SESSION_POST_LIMIT + 1u + RA_SESSION_RESPONSE_LIMIT + 1u)
#define RA_SESSION_TIMEOUT_MS 30000u
enum { RA_SESSION_COPY_OK, RA_SESSION_COPY_CANCELLED, RA_SESSION_COPY_TRANSPORT,
       RA_SESSION_COPY_BODY_LIMIT, RA_SESSION_COPY_BUFFER_LIMIT, RA_SESSION_COPY_NO_MEMORY,
       RA_SESSION_COPY_INVALID_BODY };
typedef struct ra_client_session ra_client_session;
typedef struct {
    void* context;
    uint64_t (*now_ms)(void*);
    uint32_t (*read_memory)(uint32_t, uint8_t*, uint32_t, void*);
    /* Required as a pair when a separate transport thread is used. Must not
     * fail; callbacks are never invoked while this lock is held. */
    void (*lock)(void*);
    void (*unlock)(void*);
} ra_session_config;
typedef struct {
    unsigned ticket;
    const char* url;
    const char* post_data;
    size_t post_size;
    uint64_t deadline_ms;
} ra_session_request;
typedef struct {
    unsigned submitted, completed, pending, idle_calls, frame_calls, earned_events;
    unsigned award_requests, award_confirmed, server_errors, last_earned_id, last_confirmed_id;
    unsigned event_dropped, warning_events;
    size_t buffer_bytes, peak_buffer_bytes;
    int last_result, last_copy_error;
    int logged_in, game_loaded, spectator, hardcore;
    int auth_pending, load_pending;
    unsigned game_id, console_id, achievements, unsupported, unlocked;
} ra_session_stats;
/* All methods except transport methods belong to one serialized rc_client owner.
 * Default is spectator. Online softcore must be explicitly selected BEFORE login. */
ra_client_session* ra_session_create(const ra_session_config*);
int ra_session_enable_online_softcore(ra_client_session*);
enum { RA_SESSION_EVENT_CONDITION = 1, RA_SESSION_EVENT_CONFIRMED, RA_SESSION_EVENT_REJECTED,
       RA_SESSION_EVENT_WARNING };

/* Read-only diagnostics of the pinned native rc_client runtime. No extra reads,
 * evaluation, callbacks or pointers leave the serialized owner. Bounds are explicit. */
#define RA_DIAG_CONDITIONS 32
#define RA_DIAG_REFS 8
#define RA_DIAG_SAMPLES 64
typedef struct { unsigned type, size, access, value; } ra_diag_operand;
typedef struct {
    unsigned group, type, oper, required_hits, true_frames, hit_frames, last_hits, max_hits;
    ra_diag_operand left, right;
} ra_diag_condition;
typedef struct {
    unsigned frame, time_ms, state, truths, hits, paused_groups, changed_refs;
    unsigned value[RA_DIAG_REFS], prior[RA_DIAG_REFS];
} ra_diag_sample;
typedef struct {
    unsigned id, conditions, refs, incomplete, samples, overwritten;
    unsigned addresses[RA_DIAG_REFS], sizes[RA_DIAG_REFS];
    ra_diag_condition condition[RA_DIAG_CONDITIONS];
    ra_diag_sample sample[RA_DIAG_SAMPLES], last;
} ra_session_diagnostic;
int ra_session_enable_diagnostic(ra_client_session*, unsigned id);
const ra_session_diagnostic* ra_session_get_diagnostic(const ra_client_session*);
typedef struct { unsigned id; char title[128], description[256]; } ra_session_warning;
const ra_session_warning* ra_session_get_warning(const ra_client_session*);
typedef struct { unsigned type, achievement_id, time_ms; } ra_session_event;
/* Owner-only bounded diagnostics. No strings, secrets or callback pointers. */
int ra_session_take_event(ra_client_session*, ra_session_event*);
int ra_session_achievement_state(ra_client_session*, unsigned achievement_id);
int ra_session_login_token(ra_client_session*, const char* username, const char* token);
/* The caller must compute/verify this hash from the actual image first. */
int ra_session_load_game(ra_client_session*, const char* verified_hash);
void ra_session_tick(ra_client_session*, int verified_game_frame, int paused);
/* Transport methods never call rc_client. Stop using request pointers before
 * calling complete: it publishes ownership back to the owner. No body logging. */
int ra_session_take_request(ra_client_session*, ra_session_request*);
int ra_session_request_cancelled(ra_client_session*, unsigned ticket);
int ra_session_complete(ra_client_session*, unsigned ticket, int http_status, const void*, size_t);
/* Returns 1 after freeing session/client (the pointer is then invalid).
 * Returns 0 while transport owns a request; cancel/complete and call again
 * from the owner. Never destroy a session behind a live transport. */
int ra_session_close(ra_client_session*);
void ra_session_get_stats(ra_client_session*, ra_session_stats*);
#ifdef RA_SESSION_STATE_TEST
#include "ra_state_test.h"
/* Owner thread only. Call enable once before load. Returned view is borrowed
 * until session close, and contains no raw definition or account data. */
int ra_session_enable_state_test(ra_client_session*, unsigned game_id, unsigned achievement_id);
const ra_state_test* ra_session_state_test(const ra_client_session*);
int ra_session_check_state(ra_client_session*);
#endif
#endif
