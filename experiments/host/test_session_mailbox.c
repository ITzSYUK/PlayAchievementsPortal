#include "session_mailbox.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

static const char login[] = "{\"Success\":true,\"User\":\"fixture\",\"Token\":\"synthetic-test-only\"}";
static const char game[] =
    "{\"Success\":true,\"GameId\":1,\"Title\":\"Synthetic\",\"ConsoleId\":41,"
    "\"ImageIconUrl\":\"\",\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":\"\","
    "\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Leaderboards\":[],\"Achievements\":[{"
    "\"ID\":1,\"Title\":\"Synthetic\",\"Description\":\"Synthetic\",\"Flags\":3,\"Points\":1,"
    "\"MemAddr\":\"0xH000000=1\",\"BadgeName\":\"00000\",\"Author\":\"fixture\",\"Created\":0,\"Modified\":0}]}]}";
static ra_session_mailbox mailbox;
static char response[2048];
static pthread_t owner;
static unsigned stop_network, requests, pings, network_active;
static uint64_t clock_ms;
static unsigned char game_byte;
static uint64_t now(void* ignored) { (void)ignored; assert(pthread_equal(owner,pthread_self())); return clock_ms; }
/* Regression: NO adapter lock may be taken by the transport thread, even
 * while it polls continuously during thousands of game frame evaluations. */
static void lock_owner(void* ignored) { (void)ignored; assert(pthread_equal(owner,pthread_self())); }
static uint32_t read_memory(uint32_t address,uint8_t* data,uint32_t size,void* ignored)
{ (void)ignored; lock_owner(NULL); if(address || size!=1)return 0; *data=game_byte; return 1; }
static void* transport(void* ignored)
{
    (void)ignored;
    while(!__atomic_load_n(&stop_network,__ATOMIC_ACQUIRE)) {
        ra_session_request request;
        const char* body;
        unsigned i;
        if(!ra_mail_take(&mailbox,&request)){sched_yield();continue;}
        ++requests;
        __atomic_store_n(&network_active,1,__ATOMIC_RELEASE);
        if(!strncmp(request.post_data,"r=login2&",9))body=login;
        else if(!strncmp(request.post_data,"r=achievementsets&",18))body=game;
        else if(!strncmp(request.post_data,"r=startsession&",15))body="{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}";
        else if(!strncmp(request.post_data,"r=awardachievement&",19))body="{\"Success\":true,\"AchievementID\":1,\"AchievementsRemaining\":1}";
        else {assert(!strncmp(request.post_data,"r=ping&",7));++pings;body="{\"Success\":true}";}
        /* Hold ownership across multiple owner ticks, as with delayed I/O. */
        for(i=0;i<100;++i)sched_yield();
        assert(strlen(body)<sizeof(response));strcpy(response,body);
        assert(strlen(request.post_data)==request.post_size);
        __atomic_store_n(&network_active,0,__ATOMIC_RELEASE);
        assert(ra_mail_reply(&mailbox,200,strlen(body)));
    }
    return NULL;
}
static void cancellation_cases(void)
{
    unsigned phase;
    for(phase=RA_MAIL_REQUEST;phase<=RA_MAIL_RESPONSE;++phase){
        ra_session_config config={NULL,now,read_memory,lock_owner,lock_owner};
        ra_client_session* s=ra_session_create(&config);ra_session_request request;
        memset(&mailbox,0,sizeof(mailbox));
        assert(s && ra_session_login_token(s,"fixture","synthetic-test-only"));
        assert(ra_mail_offer(&mailbox,s));
        if(phase!=RA_MAIL_REQUEST)assert(ra_mail_take(&mailbox,&request));
        if(phase==RA_MAIL_RESPONSE)assert(ra_mail_reply(&mailbox,200,sizeof(login)-1));
        assert(ra_mail_cancel_joined(&mailbox,s));assert(ra_mail_phase(&mailbox)==RA_MAIL_IDLE);
        assert(ra_session_close(s));
    }
}
int main(void)
{
    ra_session_config config={NULL,now,read_memory,lock_owner,lock_owner};
    ra_client_session* s;
    ra_session_stats stats;
    pthread_t network;
    unsigned i,frames_during_io=0,load_started=0,loaded_frames=0;
    owner=pthread_self();cancellation_cases();memset(&mailbox,0,sizeof(mailbox));
    s=ra_session_create(&config);assert(s && ra_session_enable_online_softcore(s));
    assert(ra_session_login_token(s,"fixture","synthetic-test-only"));
    assert(!pthread_create(&network,NULL,transport,NULL));
    for(i=0;i<30000;++i){
        clock_ms+=17;
        if(ra_mail_phase(&mailbox)==RA_MAIL_RESPONSE)assert(ra_mail_receive(&mailbox,s,response)==1);
        ra_session_get_stats(s,&stats);
        if(stats.logged_in && !load_started){
            assert(ra_session_load_game(s,"0123456789abcdef0123456789abcdef"));load_started=1;
        }
        if(stats.game_loaded){if(++loaded_frames>3)game_byte=1;}
        ra_session_tick(s,stats.game_loaded,!stats.game_loaded);
        if(stats.game_loaded && __atomic_load_n(&network_active,__ATOMIC_ACQUIRE))++frames_during_io;
        ra_mail_offer(&mailbox,s);
        sched_yield();
    }
    __atomic_store_n(&stop_network,1,__ATOMIC_RELEASE);
    assert(!pthread_join(network,NULL));
    if(ra_mail_phase(&mailbox)==RA_MAIL_RESPONSE)assert(ra_mail_receive(&mailbox,s,response)==1);
    ra_session_tick(s,0,1);ra_session_get_stats(s,&stats);
    assert(stats.award_confirmed==1 && stats.earned_events==1);
    assert(requests>4 && pings && frames_during_io>0 && stats.frame_calls>219);
    assert(ra_mail_cancel_joined(&mailbox,s));assert(ra_session_close(s));
    puts("PASS mailbox: real rc_client owner-only queue access; concurrent transport polling/delayed replies do not stop frames; login/set/session/award/ping >4 requests; cancellation at every handoff phase");
    return 0;
}
