#include "drivers/rtc.h"

#include "drivers/rtc/rtc_stm32_private.h"
#include "device.h"
#include "devicetree_generated.h"

bool rtc_read_datetime(RtcDateTime *date_time)
{
  return date_time != NULL &&
         device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_RTC)) &&
         RtcStm32ReadDateTime((RtcStm32DateTime *)date_time);
}
