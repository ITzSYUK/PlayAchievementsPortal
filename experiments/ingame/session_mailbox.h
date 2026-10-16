#ifndef RA_SESSION_MAILBOX_H
#define RA_SESSION_MAILBOX_H
#include "ra_client_session.h"
#include <stdint.h>
/* One bounded SPSC handoff. Only the rc_client owner calls session APIs.
 * Transport borrows request pointers until reply; body remains immutable
 * from reply until receive. Never reuse either buffer before handoff. */
enum { RA_MAIL_IDLE, RA_MAIL_REQUEST, RA_MAIL_ACTIVE, RA_MAIL_RESPONSE };
typedef struct {
    uint32_t phase;
    ra_session_request request;
    int status;
    size_t size;
} ra_session_mailbox;
static inline unsigned ra_mail_phase(const ra_session_mailbox* m)
{ return __atomic_load_n(&m->phase, __ATOMIC_ACQUIRE); }
static inline int ra_mail_offer(ra_session_mailbox* m, ra_client_session* s)
{
    if (ra_mail_phase(m) != RA_MAIL_IDLE || !ra_session_take_request(s, &m->request)) return 0;
    __atomic_store_n(&m->phase, RA_MAIL_REQUEST, __ATOMIC_RELEASE); return 1;
}
static inline int ra_mail_take(ra_session_mailbox* m, ra_session_request* request)
{
    if (ra_mail_phase(m) != RA_MAIL_REQUEST) return 0;
    *request = m->request;
    __atomic_store_n(&m->phase, RA_MAIL_ACTIVE, __ATOMIC_RELEASE); return 1;
}
static inline int ra_mail_reply(ra_session_mailbox* m, int status, size_t size)
{
    if (ra_mail_phase(m) != RA_MAIL_ACTIVE) return 0;
    m->status = status; m->size = size;
    __atomic_store_n(&m->phase, RA_MAIL_RESPONSE, __ATOMIC_RELEASE); return 1;
}
static inline int ra_mail_receive(ra_session_mailbox* m, ra_client_session* s, const void* body)
{
    int result;
    if (ra_mail_phase(m) != RA_MAIL_RESPONSE) return 0;
    result = ra_session_complete(s, m->request.ticket, m->status, body, m->size);
    __atomic_store_n(&m->phase, RA_MAIL_IDLE, __ATOMIC_RELEASE);
    return result ? 1 : -1;
}
/* Owner only, AFTER joining transport. Covers offered-but-unconsumed requests
 * as well as a response not delivered before cancellation. */
static inline int ra_mail_cancel_joined(ra_session_mailbox* m, ra_client_session* s)
{
    int result;
    if (ra_mail_phase(m) == RA_MAIL_IDLE) return 1;
    result = ra_session_complete(s, m->request.ticket, -1, NULL, 0);
    __atomic_store_n(&m->phase, RA_MAIL_IDLE, __ATOMIC_RELEASE);
    return result;
}
#endif
