#include "ra_ui.h"
#include "ra_client_session.h"
#include "ra_https.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ra_ui ui;
static uint16_t badge_pixels[32*32];
static void* producer(void* unused)
{
    unsigned i;(void)unused;
    for(i=0;i<50000;++i) {
        while(__atomic_load_n(&ui.write,__ATOMIC_ACQUIRE)-__atomic_load_n(&ui.read,__ATOMIC_ACQUIRE)==RA_UI_NOTICES){}
        ra_ui_publish(&ui,RA_SESSION_EVENT_CONDITION,1,i);
    }
    return NULL;
}
static void draw_test(void)
{
    unsigned format,menu,i;
    for(format=0;format<4;++format) {
        size_t size=512*272*(format==3?4:2);
        unsigned char* bytes=malloc(size+128);
        ra_ui_surface s={bytes+64,512,format};
        ra_ui_notice notice={RA_SESSION_EVENT_CONFIRMED,1,0};
        assert(bytes);memset(bytes,0xa5,size+128);
        for(menu=0;menu<2;++menu)ra_ui_draw(&ui,&s,(int)menu,0,999,&notice);
        for(notice.type=7;notice.type<=16;++notice.type) {
            memset(bytes,0xa5,size+128);notice.id=60;
            ra_ui_draw(&ui,&s,0,0,0,&notice);
            for(i=0;i<64;++i)assert(bytes[i]==0xa5 && bytes[size+64+i]==0xa5);
            for(i=0;i<272*512;++i) {
                unsigned x=i%512,y=i/512;
                if(x<RA_NOTICE_X || x>=RA_NOTICE_X+RA_NOTICE_WIDTH || y<RA_NOTICE_Y || y>=RA_NOTICE_Y+RA_NOTICE_HEIGHT) {
                    if(format==3)assert(((uint32_t*)(bytes+64))[i]==0xa5a5a5a5u);
                    else assert(((uint16_t*)(bytes+64))[i]==0xa5a5u);
                }
            }
        }
        ui.details=1;ra_ui_draw(&ui,&s,1,0,0,NULL);ui.details=0;
        notice.type=RA_SESSION_EVENT_CONFIRMED;notice.id=1;
        memset(bytes,0xa5,size+128);ra_ui_draw(&ui,&s,0,0,0,&notice);
        /* Top-left badge sample is pure red, including all PSP pixel formats. */
        i=(RA_NOTICE_Y+18)*512+123;
        if(format==3)assert(((uint32_t*)(bytes+64))[i]==0xff0000ffu);
        else assert(((uint16_t*)(bytes+64))[i]==(format==0?0x001f:format==1?0x801f:0xf00f));
        {
            uint16_t* tile=malloc(RA_NOTICE_WIDTH*RA_NOTICE_HEIGHT*2);
            assert(tile);for(i=0;i<RA_NOTICE_WIDTH*RA_NOTICE_HEIGHT;++i)tile[i]=0xffff;
            memset(bytes,0xa5,size+128);ra_ui_blit_notice(tile,&s);
            for(i=0;i<272*512;++i) {
                unsigned x=i%512,y=i/512;
                int inside=x>=RA_NOTICE_X && x<RA_NOTICE_X+RA_NOTICE_WIDTH && y>=RA_NOTICE_Y && y<RA_NOTICE_Y+RA_NOTICE_HEIGHT;
                if(format==3)assert(((uint32_t*)(bytes+64))[i]==(inside?0xffffffffu:0xa5a5a5a5u));
                else assert(((uint16_t*)(bytes+64))[i]==(inside?0xffffu:0xa5a5u));
            }
            free(tile);
        }
        for(i=0;i<64;++i)assert(bytes[i]==0xa5 && bytes[size+64+i]==0xa5);
        s.stride=513;assert(!ra_ui_surface_valid(&s));
        s.stride=479;assert(!ra_ui_surface_valid(&s));
        s.stride=512;s.format=4;assert(!ra_ui_surface_valid(&s));
        s.format=3;s.pixels=NULL;assert(!ra_ui_surface_valid(&s));
        free(bytes);
    }
}
static void screenshot(const char* path,int menu)
{
    uint32_t* pixels=calloc(512*272,4);ra_ui_surface s={pixels,512,3};FILE* f;unsigned i;
    ra_ui_notice notice={RA_SESSION_EVENT_CONFIRMED,1,0};
    assert(pixels);ra_ui_draw(&ui,&s,menu,0,0,&notice);f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n480 272\n255\n");
    for(i=0;i<272*480;++i) {
        unsigned color=pixels[(i/480)*512+i%480];
        unsigned char rgb[3]={(unsigned char)color,(unsigned char)(color>>8),(unsigned char)(color>>16)};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));free(pixels);
}
int main(int argc,char** argv)
{
    ra_ui_notice out;unsigned i;pthread_t thread;
    assert(ra_ui_transport_reason(0,401,0,"complete",0,RA_REQUEST_LOGIN)==RA_REASON_AUTH);
    assert(ra_ui_transport_reason(0,503,0,"complete",0,RA_REQUEST_SET)==RA_REASON_SET);
    assert(ra_ui_transport_reason(-1,0,-35425,"socket_setup",0,0)==RA_REASON_CONNECT);
    assert(ra_ui_transport_reason(-1,0,-9,"DNS_TCP",0,0)==RA_REASON_DNS);
    assert(ra_ui_transport_reason(-1,0,0,"TLS_handshake",8,0)==RA_REASON_CERTIFICATE);
    assert(ra_ui_transport_reason(-1,0,-35425,"TLS_handshake",0,0)==RA_REASON_TLS);
    assert(ra_ui_transport_reason(RA_HTTPS_TIMEOUT,0,0,"HTTP_body",0,0)==RA_REASON_TIMEOUT);
    assert(ra_ui_transport_reason(RA_HTTPS_TRUNCATED,0,0,"HTTP_body",0,0)==RA_REASON_RESPONSE);
    ra_ui_init(&ui,0);ui.count=3;
    for(i=0;i<ui.count;++i)ui.rows[i].id=i+1;
    ui.rows[2].state=RA_UI_UNSUPPORTED;
    assert(ra_ui_count(&ui,0)==3 && ra_ui_count(&ui,1)==3 && !ra_ui_count(&ui,2));
    ra_ui_publish(&ui,RA_SESSION_EVENT_CONDITION,1,1);
    assert(ra_ui_state(&ui,0)==RA_UI_PENDING && !ra_ui_count(&ui,2));
    ra_ui_publish(&ui,RA_SESSION_EVENT_REJECTED,1,2);
    assert(ra_ui_state(&ui,0)==RA_UI_REJECTED);
    ra_ui_publish(&ui,RA_SESSION_EVENT_CONFIRMED,1,3);
    assert(ra_ui_state(&ui,0)==RA_UI_CONFIRMED && ra_ui_count(&ui,2)==1);
    ra_ui_publish(&ui,RA_SESSION_EVENT_REJECTED,1,4);
    assert(ra_ui_state(&ui,0)==RA_UI_CONFIRMED);
    ra_ui_publish(&ui,RA_SESSION_EVENT_ENCORE,1,5);
    assert(ra_ui_state(&ui,0)==RA_UI_CONFIRMED);
    for(i=0;i<50;++i)ra_ui_publish(&ui,RA_SESSION_EVENT_CONDITION,2,5+i);
    assert(ui.dropped==39 && ra_ui_state(&ui,1)==RA_UI_PENDING);
    /* Even a full popup ring cannot hide a server acknowledgement. */
    ra_ui_publish(&ui,RA_SESSION_EVENT_CONFIRMED,2,60);
    assert(ra_ui_state(&ui,1)==RA_UI_CONFIRMED && ra_ui_count(&ui,2)==2);
    for(i=0;ra_ui_take(&ui,&out);++i){} assert(i==RA_UI_NOTICES);
    assert(ra_ui_index(&ui,2,1)==1 && ra_ui_index(&ui,2,2)==-1);
    ui.spectator=1;ui.rows[0].state=RA_UI_LOCKED;
    ra_ui_publish(&ui,RA_SESSION_EVENT_CONDITION,1,0);
    assert(ra_ui_state(&ui,0)==RA_UI_LOCAL);
    while(ra_ui_take(&ui,&out)){}
    assert(!pthread_create(&thread,NULL,producer,NULL));
    for(i=0;i<50000;++i) {
        while(!ra_ui_take(&ui,&out)){}
        assert(out.id==1 && out.time_ms==i && out.type==RA_SESSION_EVENT_CONDITION);
    }
    assert(!pthread_join(thread,NULL));
    strcpy(ui.rows[0].title,"Bu Bu Poruche");
    strcpy(ui.rows[0].description,"Clear the stage. Official achievement description from the server.");
    strcpy(ui.rows[1].title,"A second achievement");strcpy(ui.rows[2].title,"Unsupported condition");
    for(i=0;i<32*32;++i)badge_pixels[i]=(i%32<16?(i/32<16?0x001f:0x07e0):(i/32<16?0xf800:0xffff));
    ui.rows[0].badge=badge_pixels;
    strcpy(ui.warning_title,"Synthetic service warning");
    strcpy(ui.warning_description,"Some achievements require a fresh run. Read this warning before continuing.");
    ui.reason=RA_REASON_CERTIFICATE;ui.error_code=-0x2700;
    draw_test();
    ui.spectator=0;ui.rows[0].state=RA_UI_CONFIRMED;ui.rows[0].points=5;ui.rows[1].state=RA_UI_PENDING;
    if(argc>1)screenshot(argv[1],1);
    if(argc>2)screenshot(argv[2],0);
    puts("PASS UI: server/local/pending/rejected, filters, overflow preserves ack, 50000 concurrent publications, badge colors and top-right bounds in four framebuffer formats, warning details");
    return 0;
}
