#include "ra_auth_file.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int empty(const ra_auth* auth)
{
    const unsigned char* p = (const unsigned char*)auth;
    size_t i;
    for (i = 0; i < sizeof(*auth); ++i) if (p[i]) return 0;
    return 1;
}
int main(void)
{
    const char valid[] = "RAClientToken/1\nfixture\nsynthetic-test-only\n";
    const char crlf[] = "RAClientToken/1\r\nfixture\r\ntoken\r\n";
    const char* invalid[] = { "", "fixture\ntoken\n", "RAClientToken/2\nfixture\ntoken\n",
        "RAClientToken/1\n\ntoken\n", "RAClientToken/1\nfixture\n\n", "RAClientToken/1\nfixture\ntoken\nextra",
        "RAClientToken/1\nfixture\ntoken\n\n", "RAClientToken/1\rfixture\ntoken\n",
        "RAClientToken/1\nfi xture\ntoken\n", "RAClientToken/1\nfixture\nto\tken\n" };
    ra_auth auth;
    char large[300]; unsigned i;
    assert(ra_auth_parse(valid, sizeof(valid) - 1, &auth));
    assert(!strcmp(auth.username, "fixture") && !strcmp(auth.token, "synthetic-test-only"));
    assert(ra_auth_parse(crlf, sizeof(crlf) - 1, &auth));
    for (i = 0; i < sizeof(invalid)/sizeof(*invalid); ++i) {
        memset(&auth, 0xA5, sizeof(auth));
        assert(!ra_auth_parse(invalid[i], strlen(invalid[i]), &auth) && empty(&auth));
    }
    memset(large, 'x', sizeof(large)); memcpy(large, "RAClientToken/1\n", 15);
    large[80] = '\n'; large[90] = '\n';
    assert(!ra_auth_parse(large, 91, &auth) && empty(&auth)); /* username >64 */
    large[20] = '\n'; large[80] = 'x'; large[90] = 'x'; large[151] = '\n';
    assert(!ra_auth_parse(large, 152, &auth) && empty(&auth)); /* token >128 */
    assert(!ra_auth_parse(large, sizeof(large), &auth) && empty(&auth));
    assert(!ra_auth_parse(NULL, 1, &auth) && empty(&auth));
    assert(!ra_auth_parse(valid, sizeof(valid), &auth) && empty(&auth)); /* embedded NUL */
    assert(!ra_auth_parse(valid, sizeof(valid) - 1, NULL));
    puts("PASS token file: bounded ASCII/CRLF, invalid/oversize/partial data rejected and output erased");
    return 0;
}
