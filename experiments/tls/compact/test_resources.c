#include "ra_net_resources.h"
#include <assert.h>
#include <stdio.h>
static int fail, calls, sequence[16], args[16];
static int release(void* context, int operation, int arg)
{
    assert(context == &calls); sequence[calls] = operation; args[calls++] = arg;
    return calls == fail ? -77 : 0;
}
int main(void)
{
    ra_net_resources r;
    const int expected[] = { RA_NET_CLOSE, RA_NET_DELETE_RESOLVER, RA_NET_DISCONNECT,
        RA_NET_DELETE_APCTL_HANDLER, RA_NET_APCTL_TERM, RA_NET_RESOLVER_TERM,
        RA_NET_INET_TERM, RA_NET_TERM, RA_NET_UNLOAD, RA_NET_UNLOAD };
    int at, i;
    for (at = 1; at <= 10; ++at) {
        ra_net_resources_init(&r); r.socket = 42; r.resolver_id = 43;
        r.connected = r.apctl = r.resolver = r.inet = r.net = 1; r.modules = 2;
        r.apctl_handler = 0; /* Zero is a valid handler id, not an absent resource. */
        calls = 0; fail = at;
        assert(ra_net_resources_release(&r, release, &calls) == -77 && calls == at);
        for (i = 0; i < at; ++i) assert(sequence[i] == expected[i]);
        if (at == 4) assert(r.apctl_handler == 0 && r.apctl && r.net && r.modules == 2 && !r.connected);
        if (at == 5) assert(r.apctl_handler == -1 && r.apctl && r.net && r.modules == 2);
        fail = 0; calls = 0;
        assert(ra_net_resources_release(&r, release, &calls) == 0 && calls == 11-at);
        assert(sequence[0] == expected[at-1]);
        assert(r.socket == -1 && r.resolver_id == -1 && r.apctl_handler == -1 && !r.net && !r.modules);
        assert(ra_net_resources_release(&r, release, &calls) == 0 && calls == 11-at);
    }
    ra_net_resources_init(&r); calls = 0; r.modules = 1;
    assert(ra_net_resources_release(&r, release, &calls) == 0 && calls == 1 && args[0] == 0);
    puts("PASS network ownership: partial acquisition, every cleanup failure, dependency retention and retry");
    return 0;
}
