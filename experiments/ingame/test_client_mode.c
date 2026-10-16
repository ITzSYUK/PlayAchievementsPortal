#include "client_mode.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const char* invalid[] = {"", "softcor", "softcoreX", "SOFTCORE", " softcore", "softcore ",
        "softcore\nextra", "\xef\xbb\xbfsoftcore", "hardcore", "softcore\0extra"};
    unsigned i;
    assert(ra_client_encore_parse("1\r\n",3)==1);
    assert(ra_client_encore_parse("0",1)==0);
    assert(ra_client_encore_parse("1x",2)==-1);
    assert(ra_client_encore_parse("1\0",2)==-1);
    assert(ra_client_encore_parse("\xef\xbb\xbf" "1",4)==-1);
    assert(ra_client_encore_parse(NULL,0)==-1);
    assert(ra_client_mode_parse("softcore", 8) == 1);
    assert(ra_client_mode_parse("softcore\r\n", 10) == 1);
    assert(ra_client_mode_parse("spectator\n", 10) == 0);
    assert(ra_client_mode_parse("spectator", 9) == 0);
    assert(ra_client_mode_parse(NULL, 0) == -1);
    for (i=0;i<sizeof(invalid)/sizeof(*invalid)-1;++i)
        assert(ra_client_mode_parse(invalid[i], (unsigned)strlen(invalid[i])) == -1);
    assert(ra_client_mode_parse(invalid[i], 14) == -1);
    assert(ra_client_mode_parse("softcore\n       ", 16) == -1);
    {
        unsigned seconds = 99;
        const char* bad[] = {"", "29", "301", "-30", "120x", "120 ", " 120", "1000", "\xef\xbb\xbf" "120", "12\n0"};
        assert(ra_client_duration_parse("30",2,&seconds) && seconds==30);
        assert(ra_client_duration_parse("120\r\n",5,&seconds) && seconds==120);
        assert(ra_client_duration_parse("300\n",4,&seconds) && seconds==300);
        assert(ra_client_duration_parse("session\r\n",9,&seconds) && seconds==0);
        assert(ra_client_duration_parse("session",7,&seconds) && seconds==0);
        assert(!ra_client_duration_parse("sessionX",8,&seconds));
        assert(!ra_client_duration_parse("session\0",8,&seconds));
        for(i=0;i<sizeof(bad)/sizeof(*bad);++i) {
            seconds=99; assert(!ra_client_duration_parse(bad[i],(unsigned)strlen(bad[i]),&seconds) && seconds==99);
        }
        assert(!ra_client_duration_parse("120\0",4,&seconds));
        assert(!ra_client_duration_parse("120\r\n   ",8,&seconds));
        assert(!ra_client_duration_parse(NULL,0,&seconds));
        assert(!ra_client_duration_parse("120",3,NULL));
    }
    puts("PASS duration: session or 30..300 seconds, CRLF, invalid/truncated/oversize/BOM/NUL rejected without changing value");
    puts("PASS mode marker: only explicit softcore arms awards; spectator, truncation, BOM, case, whitespace, embedded NUL and oversized input fail closed");
    return 0;
}
