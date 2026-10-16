#include "ra_ui.h"
#include <stddef.h>
#include <string.h>
void ra_ui_blit_notice(const uint16_t* tile,const ra_ui_surface* s)
{
    unsigned x,y;
    if(!tile || !ra_ui_surface_valid(s))return;
    for(y=0;y<RA_NOTICE_HEIGHT;++y) {
        unsigned offset=(y+RA_NOTICE_Y)*s->stride+RA_NOTICE_X;
        if(s->format==0) {
            memcpy((uint16_t*)s->pixels+offset,tile+y*RA_NOTICE_WIDTH,RA_NOTICE_WIDTH*2);continue;
        }
        for(x=0;x<RA_NOTICE_WIDTH;++x) {
            unsigned p=tile[y*RA_NOTICE_WIDTH+x],r=p&31,g=(p>>5)&63,b=p>>11;
            if(s->format==3)((uint32_t*)s->pixels)[offset+x]=
                ((r<<3)|(r>>2))|(((g<<2)|(g>>4))<<8)|(((b<<3)|(b>>2))<<16)|0xff000000u;
            else ((uint16_t*)s->pixels)[offset+x]=(uint16_t)(s->format==1?
                r|((g>>1)<<5)|(b<<10)|0x8000u:(r>>1)|((g>>2)<<4)|((b>>1)<<8)|0xf000u);
        }
    }
}
/* Font from upstream plugin/src/overlay.c, pinned in dependencies.lock.json. */
static const unsigned char font[95][7] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!' - 32] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
    ['\'' - 32] = {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00},
    ['(' - 32] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    [')' - 32] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    ['+' - 32] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    [',' - 32] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08},
    ['-' - 32] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    ['.' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06},
    ['/' - 32] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},
    [':' - 32] = {0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00},
    ['?' - 32] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},

    ['0' - 32] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    ['1' - 32] = {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F},
    ['2' - 32] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    ['3' - 32] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    ['4' - 32] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    ['5' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    ['6' - 32] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    ['7' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    ['8' - 32] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    ['9' - 32] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},

    ['A' - 32] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['B' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    ['C' - 32] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    ['D' - 32] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    ['E' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    ['F' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    ['G' - 32] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    ['H' - 32] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['I' - 32] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['J' - 32] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
    ['K' - 32] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    ['L' - 32] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    ['M' - 32] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    ['N' - 32] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    ['O' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['P' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    ['Q' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    ['R' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    ['S' - 32] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    ['T' - 32] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    ['U' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['V' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    ['W' - 32] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    ['X' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    ['Y' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    ['Z' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
};
int ra_ui_surface_valid(const ra_ui_surface* s)
{ return s && s->pixels && s->stride>=480 && s->stride<=512 && s->format<=3; }
static void rect(const ra_ui_surface* s,int x,int y,int w,int h,unsigned rgb)
{
    int xx,yy;unsigned packed;
    unsigned r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x+w>480)w=480-x;
    if(y+h>272)h=272-y;
    if(w<=0 || h<=0)return;
    if(s->format==3)packed=r|(g<<8)|(b<<16)|0xff000000u;
    else if(s->format==0)packed=(r>>3)|((g>>2)<<5)|((b>>3)<<11);
    else if(s->format==1)packed=(r>>3)|((g>>3)<<5)|((b>>3)<<10)|0x8000u;
    else packed=(r>>4)|((g>>4)<<4)|((b>>4)<<8)|0xf000u;
    if(s->format==3)for(yy=y;yy<y+h;++yy) {
        volatile uint32_t* row=(volatile uint32_t*)s->pixels+(unsigned)yy*s->stride+x;
        for(xx=0;xx<w;++xx)row[xx]=packed;
    } else for(yy=y;yy<y+h;++yy) {
        volatile uint16_t* row=(volatile uint16_t*)s->pixels+(unsigned)yy*s->stride+x;
        for(xx=0;xx<w;++xx)row[xx]=(uint16_t)packed;
    }
}
static void text(const ra_ui_surface* s,int x,int y,const char* value,unsigned limit,unsigned color)
{
    unsigned i,row,col;
    for(i=0;i<limit && value[i];++i) {
        unsigned ch=(unsigned char)value[i];
        if(ch>='a' && ch<='z')ch-=32;
        if(ch<32 || ch>126)ch='?';
        for(row=0;row<7;++row)for(col=0;col<5;++col)
            if(font[ch-32][row]&(1u<<(4-col)))rect(s,x+(int)(i*6+col),y+(int)row,1,1,color);
    }
}
static void number(const ra_ui_surface* s,int x,int y,unsigned value,unsigned color)
{
    char out[11];unsigned n=10;out[n]=0;
    do{out[--n]=(char)('0'+value%10);value/=10;}while(value);
    text(s,x,y,out+n,10,color);
}
static unsigned state_color(unsigned state)
{ return state==RA_UI_CONFIRMED?0x64df98:state==RA_UI_REJECTED?0xff8585:0xd2c68a; }
void ra_ui_draw(const ra_ui* ui,const ra_ui_surface* s,int menu,unsigned filter,
                unsigned selected,const ra_ui_notice* notice)
{
    unsigned i,first,count;int index;
    if(!ui || !ra_ui_surface_valid(s))return;
    if(menu) {
        rect(s,8,6,464,260,0x0c1017);
        text(s,18,15,"RETROACHIEVEMENTS",30,0xffffff);
        text(s,270,15,ui->spectator?"SPECTATOR":ui->encore?"SOFTCORE + ENCORE":"ONLINE SOFTCORE",30,0x64df98);
        text(s,18,29,"SERVER CONFIRMED",24,0x64df98);
        number(s,126,29,ra_ui_count(ui,2),0xffffff);
        text(s,157,29,"OF",2,0xaaaaaa);number(s,178,29,ui->count,0xffffff);
        text(s,265,29,filter==0?"ALL":filter==1?"UNCONFIRMED":"CONFIRMED",30,0xd2c68a);
        text(s,18,43,"MENU PAUSE - NETWORK ACTIVE",72,0xffb878);
        if(ui->spectator)text(s,18,55,"ACCOUNT UNLOCKS NOT LOADED IN SPECTATOR",72,0xffb878);
        count=ra_ui_count(ui,filter);if(selected>=count)selected=count?count-1:0;
        first=(selected/7)*7;
        for(i=0;i<7 && first+i<count;++i) {
            unsigned state;const ra_ui_row* row;
            index=ra_ui_index(ui,filter,first+i);if(index<0)break;
            row=&ui->rows[index];state=ra_ui_state(ui,(unsigned)index);
            if(first+i==selected)rect(s,14,69+(int)i*17,452,16,0x26384c);
            text(s,20,72+(int)i*17,row->title,49,0xffffff);
            text(s,322,72+(int)i*17,state==RA_UI_CONFIRMED?"CONFIRMED":
                state==RA_UI_PENDING?"PENDING":state==RA_UI_LOCAL?"LOCAL":
                state==RA_UI_REJECTED?"ERROR":state==RA_UI_UNSUPPORTED?"UNSUPPORTED":"LOCKED",22,state_color(state));
        }
        index=ra_ui_index(ui,filter,selected);
        if(index>=0) {
            const ra_ui_row* row=&ui->rows[index];unsigned length=(unsigned)strlen(row->description);
            text(s,18,194,ra_ui_label(ra_ui_state(ui,(unsigned)index)),55,0x64df98);
            text(s,360,194,"PTS",3,0xaaaaaa);number(s,390,194,row->points,0xffffff);
            for(i=0;i<3 && i*73<length;++i)text(s,18,207+(int)i*10,row->description+i*73,73,0xdddddd);
        } else text(s,18,100,ui->count?"NO ACHIEVEMENTS IN THIS FILTER":"WAITING FOR OFFICIAL SET",60,0xdddddd);
        text(s,18,247,"TRI FILTER  UP/DN MOVE  LEFT/RIGHT PAGE  CIRCLE CLOSE",74,0xaaaaaa);
    } else if(notice) {
        if(notice->type>=7) {
            const char* heading=notice->type==7?"CONNECTING TO WI-FI":notice->type==8?"CONNECTION FAILED":
                notice->type==9?"WI-FI CONNECTED":notice->type==10?"RA LOGIN RETRY":
                notice->type==11?"RA SET LOAD RETRY":notice->type==13?"RA SERVER REACHABLE":
                notice->type==14?"ACHIEVEMENTS LOADING":"RA CONNECTION FAILED";
            rect(s,112,205,360,59,0x0c1017);rect(s,112,205,3,59,0xffb878);
            text(s,124,213,heading,55,0xffb878);
            text(s,124,227,notice->type==14?"PLEASE WAIT FOR OFFICIAL SET":notice->type==13?"REQUESTS RESUMED":notice->type==9?"LINK READY - CONTACTING RA":notice->type==12?
                "CHECK LOG AND CONFIGURATION":"RETRYING AUTOMATICALLY",55,0xffffff);
            if(notice->id){text(s,124,244,"NEXT ATTEMPT IN",20,0xaaaaaa);number(s,220,244,notice->id,0xffffff);text(s,244,244,"SEC",3,0xaaaaaa);}
            return;
        }
        const char* title=notice->type==5?"OFFICIAL SET READY":"CLIENT WARNING";
        unsigned state=RA_UI_LOCKED,points=0;
        for(i=0;i<ui->count;++i)if(ui->rows[i].id==notice->id){title=ui->rows[i].title;points=ui->rows[i].points;break;}
        if(notice->type==1)state=ui->spectator?RA_UI_LOCAL:RA_UI_PENDING;
        else if(notice->type==2)state=RA_UI_CONFIRMED;
        else if(notice->type==3)state=RA_UI_REJECTED;
        rect(s,112,205,360,59,0x0c1017);rect(s,112,205,3,59,state_color(state));
        text(s,124,213,notice->type==5?"ACHIEVEMENTS LOADED":
             notice->type==6?"ENCORE - LOCAL REPLAY":notice->type==4?"CLIENT WARNING":ra_ui_label(state),55,state_color(state));
        text(s,124,227,title,55,0xffffff);
        text(s,124,244,notice->type==2?"RETROACHIEVEMENTS ACCEPTED":notice->type==1 && !ui->spectator?
             "WAITING FOR SERVER RESPONSE":"L+R+SELECT: ACHIEVEMENTS",44,0xaaaaaa);
        if(i<ui->count) {
            text(s,398,244,"PTS",3,0xaaaaaa);number(s,428,244,points,0xffffff);
        }
    }
}
