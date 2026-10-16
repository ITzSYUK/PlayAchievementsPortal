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
int main(void)
{
    unsigned i;
    for (i = 0; i < 7; ++i) scenario(i);
    diagnostic_bounds();
    diagnostic_limits();
    puts("PASS native diagnostics: cached Delta without extra reads, idle preserves history, bounded ring/conditions/refs, warning excluded from earned/awards");
    puts("PASS online softcore: server unlocks, Delta history, idle, award/ack, rejection, retry, spectator, no duplicate award; synthetic server only");
    return 0;
}
