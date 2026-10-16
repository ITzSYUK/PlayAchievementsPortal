#include "rtc_epoch.h"
#include <stddef.h>
#include <limits.h>
int ra_rtc_epoch_seconds(uint64_t current, uint64_t epoch, uint32_t resolution, int64_t* output)
{
    uint64_t seconds;
    if (!output || !resolution || current < epoch) return -1;
    seconds = (current - epoch) / resolution;
    if (seconds > INT64_MAX) return -1;
    *output = (int64_t)seconds;
    return 0;
}
