#include "ra_client_session.h"
#include "ra_game_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t clock_ms;
static uint64_t now_ms(void* unused) { (void)unused; return clock_ms; }
static uint32_t read_segment(uint32_t address, uint8_t* bytes, uint32_t size, void* unused)
{
    (void)unused;
    assert(address >= 0x08804000 && size <= 0x08805000-address);
    memset(bytes,1,size); return size;
}
static const char login[]="{\"Success\":true,\"User\":\"fixture\",\"Token\":\"synthetic-test-only\"}";
static const char game[]=
    "{\"Success\":true,\"GameId\":1,\"Title\":\"Synthetic\",\"ConsoleId\":41,\"ImageIconUrl\":\"\","
    "\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":\"\",\"Type\":\"core\",\"ImageIconUrl\":\"\","
    "\"Leaderboards\":[],\"Achievements\":[{\"ID\":1,\"Title\":\"Synthetic\",\"Description\":\"Test\","
    "\"Flags\":3,\"Points\":1,\"MemAddr\":\"0xH00804000=1\",\"BadgeName\":\"00000\","
    "\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}]}]}";
static void test(int accessible)
{
    ra_game_map map={1,{{0x08804000,4096}}};
    ra_game_reader reader;
    ra_session_config config={&reader,now_ms,ra_game_memory_read,NULL,NULL};
    ra_session_request request;
    ra_session_stats stats;
    ra_client_session* session;
    unsigned i;
    if (!accessible) map.regions[0].address=0x08900000;
    assert(ra_game_reader_init(&reader,&map,read_segment,NULL));
    session=ra_session_create(&config);assert(session);
    assert(ra_session_login_token(session,"fixture","synthetic-test-only"));
    assert(ra_session_take_request(session,&request));
    assert(ra_session_complete(session,request.ticket,200,login,sizeof(login)-1));
    ra_session_tick(session,0,1);
#ifdef RA_SESSION_STATE_TEST
    assert(ra_session_enable_state_test(session,1,1));
    assert(!ra_session_enable_state_test(session,1,2));
#endif
    assert(ra_session_load_game(session,"0123456789abcdef0123456789abcdef"));
    assert(ra_session_take_request(session,&request));
    assert(ra_session_complete(session,request.ticket,200,game,sizeof(game)-1));
    ra_session_tick(session,0,1);ra_session_get_stats(session,&stats);
    assert(stats.last_result==RC_OK && stats.game_loaded && stats.achievements==1);
    assert(stats.unsupported==(unsigned)!accessible && stats.spectator && !stats.hardcore);
    assert(reader.calls && (accessible ? reader.accepted && !reader.rejected : reader.rejected && !reader.accepted));
#ifdef RA_SESSION_STATE_TEST
    assert(ra_session_state_test(session)->reason==RA_STATE_READY);
    assert(ra_session_check_state(session)==(accessible?1:-1));
    assert(!ra_session_state_test(session)->matched);
#endif
    /* Even a true synthetic condition and a caller claiming verified frames
     * must not trigger anything in the exact PSP NO_FRAMES configuration. */
    for(i=0;i<100;++i){++clock_ms;ra_session_tick(session,1,0);}
    ra_session_get_stats(session,&stats);
    assert(!stats.frame_calls && !stats.earned_events && !stats.pending && stats.submitted==2);
    assert(ra_session_close(session));
}
#ifdef RA_SESSION_STATE_TEST
static void capture_case(unsigned target_game,unsigned target_id,const char* body,unsigned reason,int cancel)
{
    ra_session_config config={NULL,now_ms,NULL,NULL,NULL};
    ra_client_session* s=ra_session_create(&config);
    ra_session_request request;ra_session_stats stats;
    assert(s && ra_session_enable_state_test(s,target_game,target_id));
    assert(ra_session_check_state(s)==-3);
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    assert(ra_session_take_request(s,&request));
    assert(ra_session_complete(s,request.ticket,200,login,sizeof(login)-1));ra_session_tick(s,0,1);
    assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));
    assert(ra_session_take_request(s,&request));
    if(cancel){
        assert(!ra_session_close(s));
        assert(ra_session_complete(s,request.ticket,200,body,strlen(body)));
        assert(ra_session_close(s));return;
    }
    assert(ra_session_complete(s,request.ticket,200,body,strlen(body)));ra_session_tick(s,0,1);
    assert(ra_session_state_test(s)->reason==reason);
    ra_session_get_stats(s,&stats);assert(stats.submitted==2 && !stats.pending && !stats.frame_calls && !stats.earned_events);
    assert(ra_session_close(s));
}
static void capture_boundaries(void)
{
    char history[sizeof(game)+32],duplicate[sizeof(game)*2];
    const char* def=strstr(game,"0xH00804000=1");
    const char* ach=strstr(game,"{\"ID\":1");
    const char* end=strstr(ach,"}]}]}");
    size_t prefix=(size_t)(def-game),entry=(size_t)(end-ach)+1;
    memcpy(history,game,prefix);history[prefix]='d';strcpy(history+prefix+1,def);
    capture_case(1,1,history,RA_STATE_HISTORY,0);
    capture_case(1,2,game,RA_STATE_NOT_FOUND,0);
    capture_case(2,1,game,RA_STATE_NOT_FOUND,0);
    prefix=(size_t)(end-game)+1;
    memcpy(duplicate,game,prefix);duplicate[prefix++]=',';
    memcpy(duplicate+prefix,ach,entry);strcpy(duplicate+prefix+entry,end+1);
    capture_case(1,1,duplicate,RA_STATE_UNSUPPORTED,0);
    capture_case(1,1,game,0,1);
}
#endif
int main(void)
{
    test(1);test(0);
#ifdef RA_SESSION_STATE_TEST
    capture_boundaries();
    puts("PASS target capture: exact game/core/id, missing/duplicate/history rejection, active transport cancellation, no events/requests");
#endif
    puts("PASS game session: real rc_client validates allowed segment, denied address becomes unsupported; NO_FRAMES prevents true-condition evaluation and any award request");
    return 0;
}
