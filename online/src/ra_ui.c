#include "ra_ui.h"
#include "ra_https.h"
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
const char* ra_ui_reason(unsigned reason)
{
    static const char* const labels[]={"", "WI-FI SWITCH IS OFF", "WI-FI PROFILE / ACCESS POINT",
        "DNS LOOKUP FAILED", "CANNOT CONNECT TO RA", "TLS HANDSHAKE FAILED",
        "CERTIFICATE / CLOCK CHECK FAILED", "REQUEST TIMED OUT", "LOGIN / TOKEN REJECTED",
        "OFFICIAL SET LOAD FAILED", "GAME MEMORY CHECK FAILED", "MEMORY STICK WRITE FAILED",
        "RESUME CHECK FAILED", "BADGE PACK INVALID / NO MEMORY", "SAVED QUEUE VALIDATION FAILED",
        "HTTP RESPONSE INVALID / INCOMPLETE"};
    return reason<sizeof(labels)/sizeof(labels[0])?labels[reason]:"UNKNOWN CONNECTION ERROR";
}
unsigned ra_ui_transport_reason(int code,int status,int dns,const char* stage,uint32_t verify,unsigned kind)
{
    if(verify || (stage && !strcmp(stage,"CA_parse")))return RA_REASON_CERTIFICATE;
    if(code==RA_HTTPS_TIMEOUT)return RA_REASON_TIMEOUT;
    if(!code && status>=400)return kind==RA_REQUEST_LOGIN?RA_REASON_AUTH:
        kind==RA_REQUEST_SET || kind==RA_REQUEST_START?RA_REASON_SET:RA_REASON_CONNECT;
    if(code==RA_HTTPS_LIMIT || code==RA_HTTPS_FRAMING || code==RA_HTTPS_ENCODING || code==RA_HTTPS_TRUNCATED)
        return RA_REASON_RESPONSE;
    if(stage && (!strcmp(stage,"DNS_TCP") || !strcmp(stage,"socket_setup")))
        return dns<0 && dns!=-35425?RA_REASON_DNS:RA_REASON_CONNECT;
    if(stage && (!strcmp(stage,"HTTP_send") || !strcmp(stage,"HTTP_body")))return RA_REASON_RESPONSE;
    return RA_REASON_TLS;
}
