#ifndef RA_NET_RESOURCES_H
#define RA_NET_RESOURCES_H
enum { RA_NET_CLOSE, RA_NET_DELETE_RESOLVER, RA_NET_DISCONNECT, RA_NET_APCTL_TERM,
       RA_NET_RESOLVER_TERM, RA_NET_INET_TERM, RA_NET_TERM, RA_NET_UNLOAD,
       RA_NET_DELETE_APCTL_HANDLER };
typedef struct {
    int socket, resolver_id, connected, apctl, resolver, inet, net, modules, apctl_handler;
} ra_net_resources;
void ra_net_resources_init(ra_net_resources*);
/* Caller marks ownership only after successful acquisition. Stop at first
 * cleanup failure, retain that resource and all dependencies for a retry. */
int ra_net_resources_release(ra_net_resources*, int (*operation)(void*, int, int), void*);
#endif
