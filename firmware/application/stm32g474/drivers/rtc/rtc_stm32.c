#include "drivers/rtc.h"

#include "bsp/rtc/bsp_rtc.h"

bool rtc_read_datetime(RtcDateTime *date_time)
{
  return BspRtc_ReadDateTime((BspRtcDateTime *)date_time);
}
