#include "ra_net_resources.h"
#include <string.h>
void ra_net_resources_init(ra_net_resources* r)
{ memset(r, 0, sizeof(*r)); r->socket = r->resolver_id = r->apctl_handler = -1; }
int ra_net_resources_release(ra_net_resources* r, int (*operation)(void*, int, int), void* context)
{
    int* fields[] = { &r->socket, &r->resolver_id, &r->connected, &r->apctl_handler,
        &r->apctl, &r->resolver, &r->inet, &r->net };
    const int operations[] = { RA_NET_CLOSE, RA_NET_DELETE_RESOLVER, RA_NET_DISCONNECT,
        RA_NET_DELETE_APCTL_HANDLER, RA_NET_APCTL_TERM, RA_NET_RESOLVER_TERM, RA_NET_INET_TERM, RA_NET_TERM };
    int i, rc;
    if (!operation || r->modules < 0 || r->modules > 2) return -1;
    for (i = 0; i < 8; ++i) {
        int uid = i < 2 || i == 3;
        if ((uid && *fields[i] < 0) || (!uid && !*fields[i])) continue;
        rc = operation(context, operations[i], uid ? *fields[i] : 0); if (rc < 0) return rc;
        *fields[i] = uid ? -1 : 0;
    }
    while (r->modules) {
        rc = operation(context, RA_NET_UNLOAD, r->modules - 1); if (rc < 0) return rc;
        --r->modules;
    }
    return 0;
}
