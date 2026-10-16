#include "rtc_epoch.h"
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    int64_t result = -7;
    /* Independent UTC reference: 2026-10-13 00:00:00 is Unix 1791849600. */
    const uint64_t anchor = UINT64_C(62135596800000000);
    if (ra_rtc_epoch_seconds(anchor + UINT64_C(1791849600999999), anchor, 1000000, &result) || result != 1791849600) return 1;
    if (ra_rtc_epoch_seconds(anchor, anchor, 1000000, &result) || result != 0) return 1;
    /* Beyond 2038: avoid the firmware's 32-bit time_t ABI. */
    if (ra_rtc_epoch_seconds(anchor + UINT64_C(2208988800000000), anchor, 1000000, &result) || result != INT64_C(2208988800)) return 1;
    result = -7;
    if (ra_rtc_epoch_seconds(anchor - 1, anchor, 1000000, &result) != -1 || result != -7) return 1;
    if (ra_rtc_epoch_seconds(anchor, anchor, 0, &result) != -1 || result != -7) return 1;
    if (ra_rtc_epoch_seconds(UINT64_MAX, 0, 1, &result) != -1 || result != -7) return 1;
    if (ra_rtc_epoch_seconds(anchor, anchor, 1000000, NULL) != -1) return 1;
    puts("PASS: RTC epoch conversion, fractions, 2038 and invalid input");
    return 0;
}
