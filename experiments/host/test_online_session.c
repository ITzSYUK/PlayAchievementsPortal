#include "ra_client_session.h"
#include "rc_runtime_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* All server data and credentials here are synthetic; no network is used. */
static uint64_t clock_ms;
static unsigned char memory[16];
static unsigned memory_reads;
static uint64_t now(void* context) { (void)context; return clock_ms; }
static uint32_t read_memory(uint32_t address, uint8_t* bytes, uint32_t size, void* context)
{
    (void)context;
    ++memory_reads;
    if (size != 1 || address >= sizeof(memory)) return 0;
    *bytes = memory[address]; return 1;
}
static const char login[] = "{\"Success\":true,\"User\":\"fixture\",\"Token\":\"synthetic-test-only\"}";
static const char game[] =
    "{\"Success\":true,\"GameId\":1,\"Title\":\"Synthetic\",\"ConsoleId\":41,"
    "\"ImageIconUrl\":\"\",\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":\"\","
    "\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Leaderboards\":[],\"Achievements\":[{"
    "\"ID\":1,\"Title\":\"History\",\"Description\":\"Synthetic\",\"Flags\":3,\"Points\":1,"
    "\"MemAddr\":\"0xH000000=1_d0xH000000=0\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0},{"
    "\"ID\":2,\"Title\":\"Other\",\"Description\":\"Synthetic\",\"Flags\":3,\"Points\":1,"
    "\"MemAddr\":\"0xH000001=1\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0},{"
    "\"ID\":101000001,\"Title\":\"Warning\",\"Description\":\"Synthetic warning\",\"Flags\":3,\"Points\":0,"
    "\"MemAddr\":\"1=1.3.\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}]}]}";
static void tick(ra_client_session* session, int frame)
{ clock_ms += 17; ra_session_tick(session, frame, !frame); }
static void reply(ra_client_session* s, const char* route, int status, const char* body)
{
    ra_session_request request;
    assert(ra_session_take_request(s, &request));
    while(!strncmp(request.post_data,"r=ping&",7) && strcmp(route,"r=ping&")) {
        const char pong[]="{\"Success\":true}";
        assert(ra_session_complete(s,request.ticket,200,pong,strlen(pong)));tick(s,0);
        assert(ra_session_take_request(s,&request));
    }
    assert(!strncmp(request.post_data, route, strlen(route)));
    if (!strcmp(route, "r=awardachievement&")) {
        assert(strstr(request.post_data, "&a=1&"));
        assert(strstr(request.post_data, "&h=0&"));
        assert(strstr(request.post_data, "&m=0123456789abcdef0123456789abcdef&"));
    }
    assert(ra_session_complete(s, request.ticket, status, body, strlen(body)));
    tick(s, 0);
}
static void scenario(unsigned which)
{
    ra_session_config config = {NULL, now, read_memory, NULL, NULL};
    ra_client_session* s = ra_session_create(&config);
    ra_session_stats stats;
    ra_session_request request;
    int online = which != 4;
    memset(memory, 0, sizeof(memory));
    assert(s);
    if (online) assert(ra_session_enable_online_softcore(s));
    assert(ra_session_login_token(s, "fixture", "synthetic-test-only"));
    assert(!ra_session_enable_online_softcore(s));
    reply(s, "r=login2&", 200, login);
    assert(ra_session_load_game(s, "0123456789abcdef0123456789abcdef"));
    reply(s, "r=achievementsets&", 200, game);
    if (online) reply(s, "r=startsession&", 200, which == 3 ?
        "{\"Success\":true,\"Unlocks\":[{\"ID\":1,\"When\":1}],\"HardcoreUnlocks\":[]}" :
        "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}");
    ra_session_get_stats(s, &stats);
    assert(stats.game_loaded && stats.game_id == 1 && !stats.hardcore);
    assert(stats.spectator == !online);
    {
        ra_ui ui;
        unsigned i,found=0;
        assert(ra_session_prepare_ui(s,&ui));
        assert(ui.count==2 && ui.spectator==(unsigned)!online);
        for(i=0;i<ui.count;++i) {
            assert(ui.rows[i].id<101000001u && ui.rows[i].title[0]);
            if(ui.rows[i].id==1) {
                ++found;
                assert(ui.rows[i].state==(which==3?RA_UI_CONFIRMED:RA_UI_LOCKED));
                assert(!strcmp(ui.rows[i].title,"History"));
            }
        }
        assert(found==1);
    }
    if(which==4) {
        unsigned reads=memory_reads;
        const ra_session_diagnostic* d;
        assert(ra_session_enable_diagnostic(s,1));
        assert(!ra_session_enable_diagnostic(s,2));
        d=ra_session_get_diagnostic(s);
        assert(d && !d->incomplete && d->conditions==2 && d->refs==1);
        assert(d->condition[1].left.type==RC_OPERAND_DELTA);
        assert(d->condition[0].right.value==1 && d->addresses[0]==0);
        tick(s,0);
        assert(!d->samples && memory_reads==reads);
    }
    tick(s, 1); tick(s, 1);
    memory[0] = 1;
    /* idle while memory changes must not consume Delta history */
    tick(s, 0); tick(s, 0);
    tick(s, 1);
    ra_session_get_stats(s, &stats);
    assert(!stats.award_confirmed);
    if (which == 6) {
        assert(ra_session_take_request(s, &request));
        assert(!ra_session_close(s));
        assert(ra_session_request_cancelled(s, request.ticket));
        assert(ra_session_complete(s, request.ticket, -1, NULL, 0));
        assert(ra_session_close(s));
        return;
    }
    if (which == 3) {
        assert(!stats.earned_events && !stats.award_requests);
    } else {
        assert(stats.earned_events == 1 && stats.last_earned_id == 1);
        if (online) {
            if (which == 1) reply(s, "r=awardachievement&", 503, "unavailable");
            reply(s, "r=awardachievement&", 200, which == 5 ?
                "{\"Success\":true,\"AchievementID\":99}" : which == 2 ?
                "{\"Success\":false,\"Error\":\"Synthetic rejection\"}" :
                "{\"Success\":true,\"AchievementID\":1,\"Score\":0,\"SoftcoreScore\":1,\"AchievementsRemaining\":1}");
            ra_session_get_stats(s, &stats);
            assert(stats.award_confirmed == (which != 2 && which != 5));
            assert(stats.server_errors == (which == 2 || which == 5));
            if (which != 2 && which != 5) assert(stats.last_confirmed_id == 1);
        } else assert(!stats.award_requests);
    }
    tick(s, 1); tick(s, 1);
    assert(!ra_session_take_request(s, &request)); /* no duplicate awards */
    {
        ra_session_event event;
        unsigned condition = 0, confirmed = 0, rejected = 0;
        while (ra_session_take_event(s, &event)) {
            if(event.type==RA_SESSION_EVENT_WARNING) { assert(event.achievement_id==101000001); continue; }
            assert(event.achievement_id == 1 && event.time_ms > 0);
            if (event.type == RA_SESSION_EVENT_CONDITION) ++condition;
            else if (event.type == RA_SESSION_EVENT_CONFIRMED) ++confirmed;
            else if (event.type == RA_SESSION_EVENT_REJECTED) ++rejected;
            else assert(0);
        }
        assert(condition == (which != 3));
        assert(confirmed == (which == 0 || which == 1));
        assert(rejected == (which == 2 || which == 5));
    }
    ra_session_get_stats(s,&stats);
    assert(stats.warning_events==1);
    assert(ra_session_get_warning(s)->id==101000001);
    assert(!strcmp(ra_session_get_warning(s)->title,"Warning"));
    if(which==4) {
        const ra_session_diagnostic* d=ra_session_get_diagnostic(s);
        assert(d->samples>=2 && d->last.state==RC_TRIGGER_STATE_TRIGGERED);
        assert(d->last.value[0]==1 && d->last.prior[0]==0);
        assert(d->condition[0].true_frames && d->condition[1].true_frames);
    }
    assert(ra_session_close(s));
}
static void diagnostic_bounds(void)
{
    ra_session_config config={NULL,now,read_memory,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);
    const ra_session_diagnostic* d;
    unsigned i,reads;
    memset(memory,0,sizeof(memory));
    assert(s && !ra_session_enable_diagnostic(s,1));
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    reply(s,"r=login2&",200,login);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    reply(s,"r=achievementsets&",200,game);
    reads=memory_reads;
    assert(!ra_session_enable_diagnostic(s,999));
    assert(ra_session_enable_diagnostic(s,1));
    assert(memory_reads==reads);
    d=ra_session_get_diagnostic(s);
    for(i=0;i<100;++i) { memory[0]=(unsigned char)(2+i%2); tick(s,1); }
    assert(d->samples==100 && d->overwritten==100-RA_DIAG_SAMPLES);
    assert(!d->condition[0].true_frames && d->condition[1].true_frames==1);
    assert(d->last.value[0]==3 && d->last.prior[0]==2);
    reads=memory_reads; memory[0]=0; tick(s,0);
    assert(d->samples==100 && memory_reads==reads && d->last.value[0]==3);
    tick(s,1); memory[0]=1; tick(s,1);
    assert(d->last.state==RC_TRIGGER_STATE_TRIGGERED);
    assert(d->last.value[0]==1 && d->last.prior[0]==0);
    assert(d->last.changed_refs==1);
    assert(d->condition[0].true_frames==1);
    assert(ra_session_close(s));
}
static void encore_replay(void)
{
    ra_session_config config={NULL,now,read_memory,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);
    ra_session_stats stats;ra_session_request req;ra_session_event event;ra_ui ui;
    unsigned replays=0;const char ack[]="{\"Success\":true,\"AchievementID\":2,\"SoftcoreScore\":2}";
    memset(memory,0,sizeof(memory));
    assert(s && !ra_session_enable_encore(s));
    assert(ra_session_enable_online_softcore(s) && ra_session_enable_encore(s));
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    assert(!ra_session_enable_encore(s));reply(s,"r=login2&",200,login);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    reply(s,"r=achievementsets&",200,game);
    reply(s,"r=startsession&",200,"{\"Success\":true,\"Unlocks\":[{\"ID\":1,\"When\":1}],\"HardcoreUnlocks\":[]}");
    assert(ra_session_prepare_ui(s,&ui) && ui.encore && !ui.spectator);
    tick(s,1);tick(s,1);memory[0]=1;tick(s,1);
    ra_session_get_stats(s,&stats);
    assert(stats.encore_events==1 && !stats.earned_events && !stats.award_requests && !stats.award_confirmed);
    assert(!ra_session_take_request(s,&req));
    while(ra_session_take_event(s,&event)) {
        if(event.type==RA_SESSION_EVENT_ENCORE){assert(event.achievement_id==1);++replays;}
        else assert(event.type==RA_SESSION_EVENT_WARNING);
    }
    assert(replays==1);
    memory[1]=1;tick(s,1);
    assert(ra_session_take_request(s,&req) && strstr(req.post_data,"r=awardachievement&") && strstr(req.post_data,"&a=2&"));
    assert(ra_session_complete(s,req.ticket,200,ack,strlen(ack)));tick(s,0);
    ra_session_get_stats(s,&stats);
    assert(stats.encore_events==1 && stats.earned_events==1 && stats.award_requests==1 && stats.award_confirmed==1);
    assert(ra_session_close(s));
    puts("PASS Encore: prior server unlock replays locally without award/ACK, new unlock still submits, account status retained");
}
static void diagnostic_limits(void)
{
    unsigned which,i;
    for(which=0;which<2;++which) {
        char definition[1024],body[2048];
        size_t used=0;
        ra_session_config config={NULL,now,read_memory,NULL,NULL};
        ra_client_session* s=ra_session_create(&config);
        const ra_session_diagnostic* d;
        for(i=0;i<(which?9u:33u);++i)
            used+=(size_t)snprintf(definition+used,sizeof(definition)-used,"%s0xH%06X=0",i?"_":"",which?i:0);
        assert(used<sizeof(definition));
        assert(snprintf(body,sizeof(body),
            "{\"Success\":true,\"GameId\":1,\"Title\":\"Limits\",\"ConsoleId\":41,\"ImageIconUrl\":\"\","
            "\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":\"\",\"Type\":\"core\",\"ImageIconUrl\":\"\","
            "\"Leaderboards\":[],\"Achievements\":[{\"ID\":1,\"Title\":\"Limit\",\"Description\":\"Synthetic\","
            "\"Flags\":3,\"Points\":1,\"MemAddr\":\"%s\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}]}]}",
            definition)<(int)sizeof(body));
        memset(memory,0,sizeof(memory));
        assert(s && ra_session_login_token(s,"fixture","synthetic-test-only"));
        reply(s,"r=login2&",200,login);
        assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
        reply(s,"r=achievementsets&",200,body);
        assert(ra_session_enable_diagnostic(s,1));
        d=ra_session_get_diagnostic(s);
        assert(d->incomplete && d->conditions==(which?9u:32u) && d->refs==(which?8u:1u));
        tick(s,1); assert(d->samples==1);
        assert(ra_session_close(s));
    }
}
static void recover_startup(void)
{
    ra_session_config config={NULL,now,read_memory,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);ra_session_stats stats;
    assert(s && ra_session_enable_online_softcore(s));
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    reply(s,"r=login2&",RA_SESSION_RETRYABLE,"");
    ra_session_get_stats(s,&stats);
    assert(!stats.logged_in && !stats.auth_pending && stats.last_http_status==RA_SESSION_RETRYABLE);
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    reply(s,"r=login2&",200,login);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    reply(s,"r=achievementsets&",503,"unavailable");
    ra_session_get_stats(s,&stats);
    assert(!stats.game_loaded && !stats.load_pending && stats.last_http_status==503);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    reply(s,"r=achievementsets&",200,game);
    reply(s,"r=startsession&",200,"{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}");
    ra_session_get_stats(s,&stats);assert(stats.logged_in && stats.game_loaded && !stats.last_result);
    assert(ra_session_close(s));
    puts("PASS startup recovery: failed login and set load retry in same session, no dangling callback/pending request");
}
/* Reproduce a softcore load stalled after receiving the set: startsession
 * exceeds its deadline, then a fresh load succeeds in the same client. */
static void trace_load_timeout(void)
{
    ra_session_config config={NULL,now,read_memory,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);
    ra_session_stats stats;
    ra_session_request req;
    ra_request_trace event;
    unsigned stage=0, ticket, count=0;
    assert(s && ra_session_enable_online_softcore(s));
    /* Exercise timestamp wrapping without overflowing the 64-bit clock. */
    clock_ms=UINT32_MAX-100u;
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    reply(s,"r=login2&",200,login);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    reply(s,"r=achievementsets&",200,game);
    assert(ra_session_take_request(s,&req) && req.kind==RA_REQUEST_START);
    ticket=req.ticket;
    clock_ms=req.deadline_ms+1;
    /* A late HTTP 200 must remain a retryable timeout, not a successful load. */
    assert(ra_session_complete(s,ticket,200,"{}",2));
    tick(s,0);
    ra_session_get_stats(s,&stats);
    assert(!stats.game_loaded && !stats.load_pending && stats.last_http_status==RA_SESSION_RETRYABLE);
    while(ra_session_take_request_trace(s,&event)) {
        ++count;
        if(event.ticket!=ticket)continue;
        assert(event.kind==RA_REQUEST_START && event.stage==++stage);
        if(event.stage>=RA_REQUEST_RESPONSE) {
            assert(event.status<0 && event.age_ms>=RA_SESSION_TIMEOUT_MS);
        }
    }
    assert(count<=RA_REQUEST_TRACE_CAPACITY && stage==RA_REQUEST_CALLBACK_END);
    assert(!stats.request_trace_dropped);
    /* Retry in the same client after the failed startsession. */
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    reply(s,"r=achievementsets&",200,game);
    reply(s,"r=startsession&",200,"{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}");
    tick(s,0);
    ra_session_get_stats(s,&stats);assert(stats.game_loaded && !stats.load_pending);
    /* Pending automatic ping may occupy one slot; repeated idle ticks do not
     * create unbounded traces, and diagnostics never contain POST pointers. */
    count=0;while(ra_session_take_request_trace(s,&event))++count;
    assert(count<=RA_REQUEST_TRACE_CAPACITY);
    assert(ra_session_close(s));
    s=ra_session_create(&config);assert(s);
    for(count=0;count<6;++count) {
        assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
        reply(s,"r=login2&",503,"");
    }
    ra_session_get_stats(s,&stats);
    assert(stats.completed==6 && !stats.pending && !stats.auth_pending);
    assert(stats.request_trace_dropped==30-RA_REQUEST_TRACE_CAPACITY);
    count=0;while(ra_session_take_request_trace(s,&event))++count;
    assert(count==RA_REQUEST_TRACE_CAPACITY);
    assert(ra_session_close(s));
    puts("PASS request trace: startsession late response, callback order/status, timestamp wrap, successful load retry, bounded overflow without blocked callbacks");
}
static void recover_award(void)
{
    ra_session_config config={NULL,now,read_memory,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);ra_session_stats stats;ra_session_request req;
    unsigned i;const char ack[]="{\"Success\":true,\"AchievementID\":1,\"SoftcoreScore\":1}";
    memset(memory,0,sizeof(memory));assert(s && ra_session_enable_online_softcore(s));
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));reply(s,"r=login2&",200,login);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));reply(s,"r=achievementsets&",200,game);
    reply(s,"r=startsession&",200,"{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}");
    tick(s,1);tick(s,1);memory[0]=1;tick(s,1);
    reply(s,"r=awardachievement&",RA_SESSION_RETRYABLE,"");
    assert(ra_session_take_request(s,&req) && strstr(req.post_data,"r=awardachievement&"));
    /* HTTP was in flight when Wi-Fi vanished. Expiration must remain retryable. */
    clock_ms+=RA_SESSION_TIMEOUT_MS+1;
    assert(ra_session_complete(s,req.ticket,RA_SESSION_RETRYABLE,NULL,0));tick(s,0);
    for(i=0;i<3;++i){clock_ms+=1000;tick(s,0);}
    reply(s,"r=awardachievement&",200,ack);
    ra_session_get_stats(s,&stats);
    assert(stats.earned_events==1 && stats.award_requests==3 && stats.award_confirmed==1 && !stats.server_errors);
    tick(s,1);assert(!ra_session_take_request(s,&req));assert(ra_session_close(s));
    puts("PASS award recovery: transient transport then in-flight timeout, bounded backoff, one confirmed award, no re-login or game reset");
}
static void full_offline_queue(void)
{
    ra_session_config config={NULL,now,read_memory,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);ra_session_stats stats;ra_session_request req;
    unsigned i,acknowledged=0;
    memset(memory,0,sizeof(memory));assert(s && ra_session_enable_online_softcore(s));
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));reply(s,"r=login2&",200,login);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));reply(s,"r=achievementsets&",200,game);
    reply(s,"r=startsession&",200,"{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}");
    tick(s,1);tick(s,1);
    clock_ms+=30000;tick(s,0); /* periodic ping occupies the first slot */
    memory[0]=memory[1]=1;tick(s,1); /* two awards: one initially cannot fit */
    ra_session_get_stats(s,&stats);
    assert(stats.earned_events==2 && stats.pending==RA_SESSION_SLOTS && !stats.server_errors);
    for(i=0;i<16 && acknowledged<2;++i) {
        if(ra_session_take_request(s,&req)) {
            const char* body;
            if(strstr(req.post_data,"r=ping&"))body="{\"Success\":true}";
            else if(strstr(req.post_data,"&a=1&")){body="{\"Success\":true,\"AchievementID\":1}";++acknowledged;}
            else {assert(strstr(req.post_data,"&a=2&"));body="{\"Success\":true,\"AchievementID\":2}";++acknowledged;}
            assert(ra_session_complete(s,req.ticket,200,body,strlen(body)));
        }
        clock_ms+=1000;tick(s,0);
    }
    ra_session_get_stats(s,&stats);
    assert(acknowledged==2 && stats.award_confirmed==2 && !stats.server_errors);
    assert(ra_session_close(s));
    puts("PASS full offline queue: concurrent ping and two awards, saturated slot retries without rejection or lost award");
}
int main(void)
{
    unsigned i;
    for (i = 0; i < 7; ++i) scenario(i);
    diagnostic_bounds();
    encore_replay();
    recover_startup();trace_load_timeout();recover_award();full_offline_queue();
    diagnostic_limits();
    puts("PASS native diagnostics: cached Delta without extra reads, idle preserves history, bounded ring/conditions/refs, warning excluded from earned/awards");
    puts("PASS online softcore: server unlocks, Delta history, idle, award/ack, rejection, retry, spectator, no duplicate award; synthetic server only");
    return 0;
}
