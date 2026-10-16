#include "ra_ui.h"
#include "ra_client_session.h"
#include <string.h>
void ra_ui_init(ra_ui* ui, unsigned spectator)
{ memset(ui,0,sizeof(*ui)); ui->spectator=spectator; }
unsigned ra_ui_state(const ra_ui* ui, unsigned index)
{ return index<ui->count ? __atomic_load_n(&ui->rows[index].state,__ATOMIC_ACQUIRE) : RA_UI_UNSUPPORTED; }
void ra_ui_publish(ra_ui* ui, unsigned type, unsigned id, unsigned time_ms)
{
    unsigned i,write=ui->write;
    for(i=0;i<ui->count;++i) if(ui->rows[i].id==id) {
        unsigned state=ra_ui_state(ui,i);
        if(type==RA_SESSION_EVENT_CONDITION && state!=RA_UI_CONFIRMED)
            state=ui->spectator?RA_UI_LOCAL:RA_UI_PENDING;
        else if(type==RA_SESSION_EVENT_CONFIRMED) state=RA_UI_CONFIRMED;
        else if(type==RA_SESSION_EVENT_REJECTED && state!=RA_UI_CONFIRMED) state=RA_UI_REJECTED;
        __atomic_store_n(&ui->rows[i].state,state,__ATOMIC_RELEASE);
        break;
    }
    /* Overflow can drop a popup, never the authoritative row state. */
    if(write-__atomic_load_n(&ui->read,__ATOMIC_ACQUIRE)==RA_UI_NOTICES) {
        __atomic_add_fetch(&ui->dropped,1,__ATOMIC_RELAXED); return;
    }
    ui->notices[write%RA_UI_NOTICES]=(ra_ui_notice){type,id,time_ms};
    __atomic_store_n(&ui->write,write+1,__ATOMIC_RELEASE);
}
int ra_ui_take(ra_ui* ui, ra_ui_notice* out)
{
    unsigned read=ui->read;
    if(read==__atomic_load_n(&ui->write,__ATOMIC_ACQUIRE))return 0;
    *out=ui->notices[read%RA_UI_NOTICES];
    __atomic_store_n(&ui->read,read+1,__ATOMIC_RELEASE);return 1;
}
static int matches(const ra_ui* ui,unsigned index,unsigned filter)
{ return !filter || ((ra_ui_state(ui,index)==RA_UI_CONFIRMED)==(filter==2)); }
unsigned ra_ui_count(const ra_ui* ui,unsigned filter)
{
    unsigned i,count=0;
    for(i=0;i<ui->count;++i)if(matches(ui,i,filter))++count;
    return count;
}
int ra_ui_index(const ra_ui* ui,unsigned filter,unsigned position)
{
    unsigned i;
    for(i=0;i<ui->count;++i)if(matches(ui,i,filter)) { if(!position)return (int)i; --position; }
    return -1;
}
const char* ra_ui_label(unsigned state)
{
    switch(state) {
    case RA_UI_CONFIRMED:return "SERVER CONFIRMED";
    case RA_UI_PENDING:return "PENDING SERVER";
    case RA_UI_LOCAL:return "CONDITION MET - SPECTATOR";
    case RA_UI_REJECTED:return "SERVER ERROR - UNCONFIRMED";
    case RA_UI_UNSUPPORTED:return "UNSUPPORTED";
    default:return "LOCKED";
    }
}
