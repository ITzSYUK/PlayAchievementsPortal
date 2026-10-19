#include "ra_client_session.h"
#include <stdlib.h>
#include <string.h>
#include "rc_api_runtime.h"
#include "rc_error.h"
#include "../../vendor/rcheevos/src/rc_client_internal.h"
#ifdef RA_SESSION_STATE_TEST
#include "rc_client_internal.h"
#include "rc_api_runtime.h"
#endif
#define RA_SESSION_URL "https://retroachievements.org/dorequest.php"
enum { EMPTY, QUEUED, ACTIVE, COMPLETE };
typedef struct {
    unsigned state, ticket, kind;
    unsigned award_id;
    char* post;
    size_t post_size;
    char* body;
    size_t body_size;
    uint64_t deadline_ms;
    int status;
    rc_client_server_callback_t callback;
    void* callback_data;
} session_slot;
struct ra_client_session {
    rc_client_t* client;
    ra_session_config config;
    session_slot slots[RA_SESSION_SLOTS];
    ra_session_stats stats;
    unsigned next_ticket;
    ra_request_trace request_trace[RA_REQUEST_TRACE_CAPACITY];
    unsigned trace_read, trace_write;
    int stopping, auth_pending, load_pending;
    int online_softcore;
    unsigned encore, encore_ready, initial_unlocked_count;
    unsigned initial_unlocked[RA_UI_MAX];
    ra_session_event events[256];
    unsigned event_read, event_write;
    ra_session_diagnostic* diagnostic;
    const rc_condition_t* diag_conditions[RA_DIAG_CONDITIONS];
    const rc_memref_t* diag_refs[RA_DIAG_REFS];
    const rc_trigger_t* diag_trigger;
    ra_session_warning warning;
#ifdef RA_SESSION_STATE_TEST
    ra_state_test target_test;
    unsigned target_game, target_id;
#endif
};
#ifdef RA_SESSION_STATE_TEST
/* Private ABI tied to rcheevos v12.5.0 commit 1433173220a7eaede6a9ed7a18e94117be1821e0.
 * Owner callback receives parsed official definitions; never changes the response. */
static void RC_CCONV capture_target(const rc_api_server_response_t* server_response,
    rc_api_fetch_game_sets_response_t* response, rc_client_t* client, void* userdata)
{
    ra_client_session* s=rc_client_get_userdata(client);
    const char* definition=NULL;
    unsigned i,j,matches=0;
    (void)server_response;(void)userdata;
    if(s->stopping || !response->response.succeeded || response->id!=s->target_game || response->console_id!=41)return;
    for(i=0;i<response->num_sets;++i){
        const rc_api_achievement_set_definition_t* set=&response->sets[i];
        if(set->type!=RC_ACHIEVEMENT_SET_TYPE_CORE || set->game_id!=s->target_game)continue;
        for(j=0;j<set->num_achievements;++j){
            const rc_api_achievement_definition_t* a=&set->achievements[j];
            if(a->id==s->target_id && a->category==RC_ACHIEVEMENT_CATEGORY_CORE){definition=a->definition;++matches;}
        }
    }
    if(matches==1)ra_state_test_prepare(&s->target_test,definition);
    else if(matches>1)s->target_test.reason=RA_STATE_UNSUPPORTED;
}
int ra_session_enable_state_test(ra_client_session* s,unsigned game_id,unsigned achievement_id)
{
    if(!s || s->stopping || s->load_pending || s->target_id || !game_id || !achievement_id || rc_client_get_game_info(s->client))return 0;
    s->target_game=game_id;s->target_id=achievement_id;
    s->client->callbacks.post_process_game_sets_response=capture_target;
    return 1;
}
const ra_state_test* ra_session_state_test(const ra_client_session* s)
{ return s?&s->target_test:NULL; }
int ra_session_check_state(ra_client_session* s)
{
    const rc_client_game_t* game;
    if(!s || s->stopping || !rc_client_get_spectator_mode_enabled(s->client) || rc_client_get_hardcore_enabled(s->client))return -3;
    game=rc_client_get_game_info(s->client);
    if(!game || game->id!=s->target_game || game->console_id!=41)return -3;
    return ra_state_test_check(&s->target_test,s->config.read_memory,s->config.context);
}
#endif
static void lock(ra_client_session* s) { if (s->config.lock) s->config.lock(s->config.context); }
static void unlock(ra_client_session* s) { if (s->config.unlock) s->config.unlock(s->config.context); }
static void record_buffer_peak(ra_client_session* s)
{ if (s->stats.buffer_bytes > s->stats.peak_buffer_bytes) s->stats.peak_buffer_bytes = s->stats.buffer_bytes; }
static void erase_free(char* bytes, size_t size)
{
    if (bytes) {
        volatile unsigned char* p = (volatile unsigned char*)bytes;
        while (size--) *p++ = 0;
        free(bytes);
    }
}
static size_t bounded_length(const char* text, size_t maximum)
{
    size_t size = 0;
    if (!text) return maximum + 1;
    while (size <= maximum && text[size]) ++size;
    return size;
}
static rc_clock_t RC_CCONV client_clock(const rc_client_t* client)
{
    const ra_client_session* s = rc_client_get_userdata(client);
    return s->config.now_ms(s->config.context);
}
static uint32_t RC_CCONV client_read(uint32_t address, uint8_t* bytes, uint32_t size, rc_client_t* client)
{
    ra_client_session* s = rc_client_get_userdata(client);
    uint32_t count = s->config.read_memory ? s->config.read_memory(address, bytes, size, s->config.context) : 0;
    return count == size ? count : 0;
}
static void event_push(ra_client_session* s, unsigned type, unsigned id)
{
    ra_session_event* event;
    if (s->event_write-s->event_read == 256u) { ++s->stats.event_dropped; return; }
    event = &s->events[s->event_write++ % 256u];
    event->type = type; event->achievement_id = id;
    event->time_ms = (unsigned)s->config.now_ms(s->config.context);
}
static int initial_unlock(const ra_client_session* s,unsigned id)
{
    unsigned i;
    for(i=0;i<s->initial_unlocked_count;++i)if(s->initial_unlocked[i]==id)return 1;
    return 0;
}
/* Private callback ABI pinned by dependencies.lock.json, as diagnostics are.
 * Fail closed before the initial server snapshot exists. No synthetic ACK. */
static int RC_CCONV encore_can_submit(uint32_t id,rc_client_t* client)
{
    const ra_client_session* s=rc_client_get_userdata(client);
    return s->encore_ready && !initial_unlock(s,id);
}
int ra_session_take_event(ra_client_session* s, ra_session_event* event)
{
    if (!s || !event || s->event_read == s->event_write) return 0;
    *event = s->events[s->event_read++ % 256u]; return 1;
}
int ra_session_achievement_state(ra_client_session* s, unsigned id)
{
    const rc_client_achievement_t* achievement;
    if (!s || s->stopping) return -1;
    achievement = rc_client_get_achievement_info(s->client, id);
    return achievement ? (int)achievement->state : -1;
}
static void diagnostic_text(char* out, size_t capacity, const char* text)
{
    size_t i=0;
    if(text) for(;i+1<capacity && text[i];++i)
        out[i]=(unsigned char)text[i]<32 || (unsigned char)text[i]==127?' ':text[i];
    out[i]=0;
}
int ra_session_prepare_ui(ra_client_session* s, ra_ui* ui)
{
    rc_client_achievement_list_t* list;
    unsigned b,i;
    if(!s || !ui || s->stopping || s->load_pending || s->stats.frame_calls ||
       !rc_client_get_game_info(s->client))return 0;
    memset(ui,0,sizeof(*ui)); ui->spectator=(unsigned)!s->online_softcore;
    ui->encore=s->encore;
    list=rc_client_create_achievement_list(s->client,RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
                                          RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if(!list)return 0;
    for(b=0;b<list->num_buckets;++b)for(i=0;i<list->buckets[b].num_achievements;++i) {
        const rc_client_achievement_t* a=list->buckets[b].achievements[i];
        ra_ui_row* row;
        if(a->id>=101000001u)continue; /* client warning, not an achievement */
        if(ui->count==RA_UI_MAX) { ui->count=0; rc_client_destroy_achievement_list(list); return 0; }
        row=&ui->rows[ui->count++]; row->id=a->id; row->points=a->points;
        /* Spectator does not fetch account unlocks in pinned rc_client.
         * Never present its local trigger state as server confirmation. */
        row->state=!ui->spectator && a->unlocked?RA_UI_CONFIRMED:
            a->state==RC_CLIENT_ACHIEVEMENT_STATE_DISABLED?RA_UI_UNSUPPORTED:RA_UI_LOCKED;
        diagnostic_text(row->title,sizeof(row->title),a->title);
        diagnostic_text(row->description,sizeof(row->description),a->description);
    }
    rc_client_destroy_achievement_list(list);
    return ui->count!=0;
}
const ra_session_warning* ra_session_get_warning(const ra_client_session* s)
{ return s && s->warning.id?&s->warning:NULL; }
static void RC_CCONV client_event(const rc_client_event_t* event, rc_client_t* client)
{
    ra_client_session* s = rc_client_get_userdata(client);
    if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED) {
        /* Same reserved range as pinned rc_client.c: these never send awards. */
        if (event->achievement && event->achievement->id >= 101000001u) {
            ++s->stats.warning_events;
            s->warning.id=event->achievement->id;
            diagnostic_text(s->warning.title,sizeof(s->warning.title),event->achievement->title);
            diagnostic_text(s->warning.description,sizeof(s->warning.description),event->achievement->description);
            event_push(s, RA_SESSION_EVENT_WARNING, event->achievement->id);
            return;
        }
        if(s->encore && event->achievement && initial_unlock(s,event->achievement->id)) {
            ++s->stats.encore_events;
            event_push(s,RA_SESSION_EVENT_ENCORE,event->achievement->id);return;
        }
        ++s->stats.earned_events;
        s->stats.last_earned_id = event->achievement ? event->achievement->id : 0;
        event_push(s, RA_SESSION_EVENT_CONDITION, s->stats.last_earned_id);
    }
    if (event->type == RC_CLIENT_EVENT_SERVER_ERROR) {
        ++s->stats.server_errors;
        event_push(s, RA_SESSION_EVENT_REJECTED, event->server_error ? event->server_error->related_id : 0);
    }
}
/* Native private ABI pinned to rcheevos 12.5.0 / 1433173220a7eaede6a9ed7a18e94117be1821e0.
 * public_ is the first member. External clients are deliberately unsupported. */
static ra_diag_operand diagnostic_operand(ra_client_session* s, const rc_operand_t* op)
{
    ra_diag_operand out;
    ra_session_diagnostic* d=s->diagnostic;
    unsigned i;
    out.type=op->type; out.size=op->size; out.access=op->memref_access_type;
    out.value=op->value.num;
    if (rc_operand_is_memref(op)) {
        out.value=op->value.memref->address;
        for(i=0;i<d->refs;++i) if(s->diag_refs[i]==op->value.memref) return out;
        if(d->refs==RA_DIAG_REFS) d->incomplete=1;
        else {
            i=d->refs++; s->diag_refs[i]=op->value.memref;
            d->addresses[i]=out.value; d->sizes[i]=op->value.memref->value.size;
        }
    } else if(op->type!=RC_OPERAND_CONST) d->incomplete=1;
    return out;
}
int ra_session_enable_diagnostic(ra_client_session* s, unsigned id)
{
    const rc_client_achievement_info_t* a;
    const rc_condset_t* group;
    unsigned g=0;
    ra_session_diagnostic* d;
    if(!s || s->stopping || s->diagnostic || s->stats.frame_calls) return 0;
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
    if(s->client->state.external_client) return 0;
#endif
    a=(const rc_client_achievement_info_t*)rc_client_get_achievement_info(s->client,id);
    if(!a || !a->trigger) return 0;
    d=calloc(1,sizeof(*d)); if(!d) return 0;
    s->diagnostic=d; d->id=id; s->diag_trigger=a->trigger;
    group=a->trigger->requirement;
    for(;;) {
        const rc_condition_t* c=group?group->conditions:NULL;
        if(g>=32) d->incomplete=1;
        for(;c;c=c->next) {
            ra_diag_condition* out;
            if(d->conditions==RA_DIAG_CONDITIONS) { d->incomplete=1; break; }
            s->diag_conditions[d->conditions]=c; out=&d->condition[d->conditions++];
            out->group=g; out->type=c->type; out->oper=c->oper; out->required_hits=c->required_hits;
            out->left=diagnostic_operand(s,&c->operand1); out->right=diagnostic_operand(s,&c->operand2);
        }
        group=g?group->next:a->trigger->alternative; ++g;
        if(!group) break;
    }
    return 1;
}
const ra_session_diagnostic* ra_session_get_diagnostic(const ra_client_session* s)
{ return s?s->diagnostic:NULL; }
#ifndef RA_SESSION_NO_FRAMES
static void diagnostic_frame(ra_client_session* s)
{
    ra_session_diagnostic* d=s->diagnostic;
    ra_diag_sample sample;
    const rc_condset_t* group;
    unsigned i,g=0;
    if(!d) return;
    memset(&sample,0,sizeof(sample)); sample.state=s->diag_trigger->state;
    for(i=0;i<d->conditions;++i) {
        const rc_condition_t* c=s->diag_conditions[i];
        if(c->is_true&1) { sample.truths|=1u<<i; ++d->condition[i].true_frames; }
        if(c->current_hits) { sample.hits|=1u<<i; ++d->condition[i].hit_frames; }
        d->condition[i].last_hits=c->current_hits;
        if(c->current_hits>d->condition[i].max_hits) d->condition[i].max_hits=c->current_hits;
    }
    group=s->diag_trigger->requirement;
    for(;;) {
        if(group && group->is_paused && g<32) sample.paused_groups|=1u<<g;
        group=g?group->next:s->diag_trigger->alternative; ++g;
        if(!group) break;
    }
    for(i=0;i<d->refs;++i) {
        sample.value[i]=s->diag_refs[i]->value.value;
        sample.prior[i]=s->diag_refs[i]->value.prior;
        if(s->diag_refs[i]->value.changed) sample.changed_refs|=1u<<i;
    }
    /* Compare payload only; timestamps must not create one record per frame. */
    if(!d->samples || memcmp(&sample.state,&d->last.state,sizeof(sample)-offsetof(ra_diag_sample,state))) {
        sample.frame=s->stats.frame_calls; sample.time_ms=(unsigned)s->config.now_ms(s->config.context);
        d->sample[d->samples%RA_DIAG_SAMPLES]=sample;
        if(d->samples>=RA_DIAG_SAMPLES) ++d->overwritten;
        ++d->samples;
    } else { sample.frame=s->stats.frame_calls; sample.time_ms=(unsigned)s->config.now_ms(s->config.context); }
    d->last=sample;
}
#endif
/* Parse only a bounded decimal achievement ID. Never log POSTs or tokens. */
static unsigned award_id(const char* post)
{
    const char* p = strstr(post, "&a="); unsigned id = 0;
    if (!p) return 0;
    for (p += 3; *p >= '0' && *p <= '9'; ++p) {
        unsigned digit = (unsigned)(*p - '0');
        if (id > (UINT32_MAX - digit) / 10u) return 0;
        id = id * 10u + digit;
    }
    return (*p == '&' || !*p) ? id : 0;
}
/* Caller holds the adapter lock. Timestamp arithmetic intentionally wraps at
 * 32-bit milliseconds; request lifetimes are bounded to 30 seconds. */
static void request_trace(ra_client_session* s, const session_slot* slot, unsigned stage)
{
    ra_request_trace* event;
    unsigned now=(unsigned)s->config.now_ms(s->config.context);
    if(s->trace_write-s->trace_read==RA_REQUEST_TRACE_CAPACITY) {
        ++s->trace_read; ++s->stats.request_trace_dropped;
    }
    event=&s->request_trace[s->trace_write++ % RA_REQUEST_TRACE_CAPACITY];
    *event=(ra_request_trace){slot->ticket,slot->kind,stage,now,
        now-(unsigned)(slot->deadline_ms-RA_SESSION_TIMEOUT_MS),slot->status};
}
int ra_session_take_request_trace(ra_client_session* s, ra_request_trace* output)
{
    int found=0;
    if(!s || !output)return 0;
    lock(s);
    if(s->trace_read!=s->trace_write) {
        *output=s->request_trace[s->trace_read++ % RA_REQUEST_TRACE_CAPACITY]; found=1;
    }
    unlock(s);return found;
}
static void RC_CCONV server_call(const rc_api_request_t* request, rc_client_server_callback_t callback,
    void* data, rc_client_t* client)
{
    ra_client_session* s = rc_client_get_userdata(client);
    rc_api_server_response_t failure = { "", 0, RC_API_SERVER_RESPONSE_CLIENT_ERROR };
    size_t size = request->post_data ? bounded_length(request->post_data, RA_SESSION_POST_LIMIT) : 0;
    unsigned i;
    /* Reject credentials over plain HTTP and all unexpected hosts/paths. */
    if (!request->url || strcmp(request->url, RA_SESSION_URL) || size > RA_SESSION_POST_LIMIT ||
        !request->post_data ||
        (strncmp(request->post_data, "r=login2&", 9) &&
         strncmp(request->post_data, "r=achievementsets&", 18) &&
         !(s->online_softcore &&
           (!strncmp(request->post_data, "r=startsession&", 15) ||
            !strncmp(request->post_data, "r=ping&", 7) ||
            (!strncmp(request->post_data, "r=awardachievement&", 19) &&
             strstr(request->post_data, "&h=0&") && award_id(request->post_data)))))) {
        callback(&failure, data); return;
    }
    lock(s);
    /* A full bounded transport queue is temporary backpressure, not rejection
     * of the achievement. rc_client retains its bounded per-achievement retry. */
    if(!s->stopping)failure.http_status_code=RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
    if (!s->stopping && s->stats.buffer_bytes <= RA_SESSION_BUFFER_LIMIT &&
        size + 1 <= RA_SESSION_BUFFER_LIMIT - s->stats.buffer_bytes) {
        for (i = 0; i < RA_SESSION_SLOTS; ++i) {
            session_slot* slot = &s->slots[i];
            if (slot->state != EMPTY) continue;
            if (request->post_data) {
                slot->post = malloc(size + 1);
                if (!slot->post) {failure.http_status_code=RC_API_SERVER_RESPONSE_CLIENT_ERROR;break;}
                memcpy(slot->post, request->post_data, size + 1);
                slot->post_size = size;
                s->stats.buffer_bytes += size + 1;
                record_buffer_peak(s);
            }
            slot->callback = callback; slot->callback_data = data;
            slot->award_id = !strncmp(request->post_data, "r=awardachievement&", 19) ? award_id(request->post_data) : 0;
            if (slot->award_id) ++s->stats.award_requests;
            slot->ticket = ++s->next_ticket;
            if (!slot->ticket) slot->ticket = ++s->next_ticket;
            slot->deadline_ms = s->config.now_ms(s->config.context) + RA_SESSION_TIMEOUT_MS;
            slot->kind = !strncmp(request->post_data,"r=login2&",9)?RA_REQUEST_LOGIN:
                !strncmp(request->post_data,"r=achievementsets&",18)?RA_REQUEST_SET:
                !strncmp(request->post_data,"r=startsession&",15)?RA_REQUEST_START:
                !strncmp(request->post_data,"r=ping&",7)?RA_REQUEST_PING:RA_REQUEST_AWARD;
            slot->state = QUEUED;
            ++s->stats.pending; ++s->stats.submitted;
            request_trace(s,slot,RA_REQUEST_QUEUED);
            unlock(s); return;
        }
    }
    unlock(s);
    callback(&failure, data);
}
ra_client_session* ra_session_create(const ra_session_config* config)
{
    ra_client_session* s;
    if (!config || !config->now_ms || (!!config->lock != !!config->unlock)) return NULL;
    s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->config = *config;
    s->client = rc_client_create(client_read, server_call);
    if (!s->client) { free(s); return NULL; }
    rc_client_set_userdata(s->client, s);
    rc_client_set_host(s->client, "https://retroachievements.org");
    rc_client_set_get_time_millisecs_function(s->client, client_clock);
    rc_client_set_hardcore_enabled(s->client, 0);
    rc_client_set_spectator_mode_enabled(s->client, 1);
    rc_client_set_allow_background_memory_reads(s->client, 0);
    rc_client_set_event_handler(s->client, client_event);
    /* Do not enable library request/error logging: it may include secrets. */
    return s;
}
int ra_session_enable_encore(ra_client_session* s)
{
    if(!s || !s->online_softcore || s->stopping || s->auth_pending || s->load_pending ||
       rc_client_get_user_info(s->client) || rc_client_get_game_info(s->client))return 0;
    s->encore=1;
    rc_client_set_encore_mode_enabled(s->client,1);
    s->client->callbacks.can_submit_achievement_unlock=encore_can_submit;
    return 1;
}
int ra_session_enable_online_softcore(ra_client_session* s)
{
#if defined(RA_SESSION_ONLINE) && !defined(RA_SESSION_NO_FRAMES)
    if (!s || s->stopping || s->auth_pending || s->load_pending ||
        rc_client_get_user_info(s->client) || rc_client_get_game_info(s->client)) return 0;
    s->online_softcore = 1;
    rc_client_set_hardcore_enabled(s->client, 0);
    rc_client_set_spectator_mode_enabled(s->client, 0);
    return 1;
#else
    (void)s; return 0;
#endif
}
static void RC_CCONV auth_complete(int result, const char* message, rc_client_t* client, void* data)
{
    ra_client_session* s = data;
    (void)message; (void)client;
    s->auth_pending = 0; s->stats.last_result = result;
}
static void RC_CCONV game_complete(int result, const char* message, rc_client_t* client, void* data)
{
    ra_client_session* s = data;
    (void)message; (void)client;
    s->load_pending = 0; s->stats.last_result = result;
    if(result==RC_OK && s->encore) {
        unsigned b,i;
        rc_client_achievement_list_t* list=rc_client_create_achievement_list(client,
            RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
        if(!list){s->stats.last_result=RC_OUT_OF_MEMORY;return;}
        s->initial_unlocked_count=0;
        for(b=0;b<list->num_buckets;++b)for(i=0;i<list->buckets[b].num_achievements;++i) {
            const rc_client_achievement_t* a=list->buckets[b].achievements[i];
            if(a->id>=101000001u || !a->unlocked)continue;
            if(s->initial_unlocked_count==RA_UI_MAX) {
                s->stats.last_result=RC_INSUFFICIENT_BUFFER;
                rc_client_destroy_achievement_list(list);return;
            }
            s->initial_unlocked[s->initial_unlocked_count++]=a->id;
        }
        rc_client_destroy_achievement_list(list);s->encore_ready=1;
    }
}
int ra_session_login_token(ra_client_session* s, const char* username, const char* token)
{
    if (!s || s->stopping || s->auth_pending || s->load_pending ||
        !bounded_length(username, 64) || bounded_length(username, 64) > 64 ||
        !bounded_length(token, 128) || bounded_length(token, 128) > 128) return 0;
    s->auth_pending = 1;
    (void)rc_client_begin_login_with_token(s->client, username, token, auth_complete, s);
    /* A synchronous callback legitimately returns no async handle. */
    return s->auth_pending || s->stats.last_result == RC_OK;
}
int ra_session_load_game(ra_client_session* s, const char* hash)
{
    unsigned i;
    if (!s || s->stopping || s->auth_pending || s->load_pending ||
        !rc_client_get_user_info(s->client) || bounded_length(hash, 32) != 32) return 0;
    for (i = 0; i < 32; ++i)
        if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return 0;
    s->load_pending = 1;
    (void)rc_client_begin_load_game(s->client, hash, game_complete, s);
    return s->load_pending || s->stats.last_result == RC_OK;
}
int ra_session_take_request(ra_client_session* s, ra_session_request* output)
{
    unsigned i;
    if (!s || !output) return 0;
    lock(s);
    if (!s->stopping) for (i = 0; i < RA_SESSION_SLOTS; ++i) {
        session_slot* slot = &s->slots[i];
        if (slot->state != QUEUED) continue;
        slot->state = ACTIVE;
        output->ticket = slot->ticket; output->url = RA_SESSION_URL;
        output->kind = slot->kind;
        output->post_data = slot->post; output->post_size = slot->post_size; output->deadline_ms = slot->deadline_ms;
        request_trace(s,slot,RA_REQUEST_OFFERED);
        unlock(s); return 1;
    }
    unlock(s); return 0;
}
int ra_session_request_cancelled(ra_client_session* s, unsigned ticket)
{
    unsigned i;
    int cancelled = 1;
    if (!s || !ticket) return 1;
    lock(s);
    for (i = 0; i < RA_SESSION_SLOTS; ++i) if (s->slots[i].state == ACTIVE && s->slots[i].ticket == ticket) {
        cancelled = s->stopping || s->config.now_ms(s->config.context) >= s->slots[i].deadline_ms;
        break;
    }
    unlock(s); return cancelled;
}
int ra_session_complete(ra_client_session* s, unsigned ticket, int status, const void* body, size_t size)
{
    unsigned i;
    if (!s || !ticket) return 0;
    lock(s);
    for (i = 0; i < RA_SESSION_SLOTS; ++i) {
        session_slot* slot = &s->slots[i];
        if (slot->state != ACTIVE || slot->ticket != ticket) continue;
        slot->status = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
        s->stats.last_copy_error = RA_SESSION_COPY_OK;
        if (s->stopping)
            s->stats.last_copy_error = RA_SESSION_COPY_CANCELLED;
        else if(status==RA_SESSION_RETRYABLE || (status>=100 && s->config.now_ms(s->config.context) >= slot->deadline_ms)) {
            slot->status=RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
            s->stats.last_copy_error=RA_SESSION_COPY_TRANSPORT;
        }
        else if (status < 100 || status > 599) s->stats.last_copy_error = RA_SESSION_COPY_TRANSPORT;
        else if (size > RA_SESSION_RESPONSE_LIMIT) s->stats.last_copy_error = RA_SESSION_COPY_BODY_LIMIT;
        else if (!body && size) s->stats.last_copy_error = RA_SESSION_COPY_INVALID_BODY;
        else if (s->stats.buffer_bytes > RA_SESSION_BUFFER_LIMIT ||
                 size + 1 > RA_SESSION_BUFFER_LIMIT - s->stats.buffer_bytes)
            s->stats.last_copy_error = RA_SESSION_COPY_BUFFER_LIMIT;
        else {
            slot->body = malloc(size + 1);
            if (slot->body) {
                if (size) memcpy(slot->body, body, size);
                slot->body[size] = 0; slot->body_size = size; slot->status = status;
                s->stats.buffer_bytes += size + 1;
                record_buffer_peak(s);
            }
            else s->stats.last_copy_error = RA_SESSION_COPY_NO_MEMORY;
        }
        slot->state = COMPLETE;
        request_trace(s,slot,RA_REQUEST_RESPONSE);
        unlock(s); return 1;
    }
    unlock(s); return 0;
}
static void pump(ra_client_session* s)
{
    unsigned i;
    for (i = 0; i < RA_SESSION_SLOTS; ++i) {
        session_slot done;
        rc_api_server_response_t response;
        size_t post_bytes, body_bytes;
        lock(s);
        if (s->slots[i].state == QUEUED && s->config.now_ms(s->config.context) >= s->slots[i].deadline_ms) {
            s->slots[i].status = s->stopping?RC_API_SERVER_RESPONSE_CLIENT_ERROR:RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
            s->slots[i].state = COMPLETE;
            request_trace(s,&s->slots[i],RA_REQUEST_RESPONSE);
        }
        if (s->slots[i].state != COMPLETE) { unlock(s); continue; }
        done = s->slots[i]; memset(&s->slots[i], 0, sizeof(s->slots[i]));
        s->stats.last_http_status=done.status==RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR?RA_SESSION_RETRYABLE:done.status;
        post_bytes = done.post ? done.post_size + 1 : 0;
        body_bytes = done.body ? done.body_size + 1 : 0;
        --s->stats.pending; ++s->stats.completed;
        erase_free(done.post, done.post_size + 1);
        s->stats.buffer_bytes -= post_bytes;
        request_trace(s,&done,RA_REQUEST_CALLBACK_BEGIN);
        unlock(s);
        response.body = done.body ? done.body : ""; response.body_length = done.body_size; response.http_status_code = done.status;
        if (done.award_id && response.http_status_code == 200) {
            rc_api_award_achievement_response_t award;
            int result = rc_api_process_award_achievement_server_response(&award, &response);
            if (result == 0 && award.response.succeeded && award.awarded_achievement_id == done.award_id) {
                ++s->stats.award_confirmed;
                s->stats.last_confirmed_id = done.award_id;
                event_push(s, RA_SESSION_EVENT_CONFIRMED, done.award_id);
            } else if (result == 0 && award.response.succeeded) {
                ++s->stats.server_errors;
                event_push(s, RA_SESSION_EVENT_REJECTED, done.award_id);
            }
            rc_api_destroy_award_achievement_response(&award);
        }
        done.callback(&response, done.callback_data);
        lock(s);request_trace(s,&done,RA_REQUEST_CALLBACK_END);unlock(s);
        erase_free(done.body, done.body_size + 1);
        lock(s); s->stats.buffer_bytes -= body_bytes; unlock(s);
    }
}
void ra_session_tick(ra_client_session* s, int verified_frame, int paused)
{
    if (!s || s->stopping) return;
    pump(s);
#ifndef RA_SESSION_NO_FRAMES
    if (verified_frame && !paused) { ++s->stats.frame_calls; rc_client_do_frame(s->client); diagnostic_frame(s); }
    else { ++s->stats.idle_calls; rc_client_idle(s->client); }
#else
    (void)verified_frame; (void)paused;
    ++s->stats.idle_calls; rc_client_idle(s->client);
#endif
}
int ra_session_close(ra_client_session* s)
{
    unsigned i, pending;
    if (!s) return 1;
    lock(s); s->stopping = 1;
    for (i = 0; i < RA_SESSION_SLOTS; ++i) if (s->slots[i].state == QUEUED || s->slots[i].state == COMPLETE) {
        session_slot* slot = &s->slots[i];
        size_t bytes = slot->body ? slot->body_size + 1 : 0;
        erase_free(slot->body, bytes); slot->body = NULL; slot->body_size = 0;
        s->stats.buffer_bytes -= bytes;
        slot->status = RC_API_SERVER_RESPONSE_CLIENT_ERROR; slot->state = COMPLETE;
    }
    unlock(s);
    pump(s);
    lock(s); pending = s->stats.pending; unlock(s);
    if (pending) return 0;
    rc_client_logout(s->client);
    rc_client_destroy(s->client);
#ifdef RA_SESSION_STATE_TEST
    ra_state_test_destroy(&s->target_test);
#endif
    free(s->diagnostic);
    free(s);
    return 1;
}
void ra_session_get_stats(ra_client_session* s, ra_session_stats* output)
{
    const rc_client_game_t* game;
    rc_client_user_game_summary_t summary;
    if (!s || !output) return;
    lock(s); *output = s->stats; unlock(s);
    output->logged_in = rc_client_get_user_info(s->client) != NULL;
    output->game_loaded = rc_client_get_game_info(s->client) != NULL;
    output->spectator = rc_client_get_spectator_mode_enabled(s->client);
    output->hardcore = rc_client_get_hardcore_enabled(s->client);
    output->auth_pending = s->auth_pending; output->load_pending = s->load_pending;
    game = rc_client_get_game_info(s->client);
    output->game_id = game ? game->id : 0;
    output->console_id = game ? game->console_id : 0;
    memset(&summary, 0, sizeof(summary));
    if (game) rc_client_get_user_game_summary(s->client, &summary);
    output->achievements = summary.num_core_achievements;
    output->unsupported = summary.num_unsupported_achievements;
    output->unlocked = summary.num_unlocked_achievements;
}
