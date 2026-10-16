#ifndef RA_RTC_EPOCH_H
#define RA_RTC_EPOCH_H
#include <stdint.h>
/* Epoch anchor and resolution come from sceRtcGetTick(1970-01-01 UTC) and
 * sceRtcGetTickResolution, avoiding firmware time_t width assumptions. */
int ra_rtc_epoch_seconds(uint64_t current, uint64_t epoch, uint32_t resolution, int64_t* output);
#endif
