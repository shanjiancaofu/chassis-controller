#include "drivers/rtc.h"

#include "drivers/rtc/rtc_stm32_private.h"
#include "device.h"
#include "devicetree_generated.h"
#include "rtc.h"

bool rtc_read_datetime(RtcDateTime *date_time)
{
  const struct device *device = DEVICE_DT_GET(DT_CHOSEN_CHASSIS_RTC);
  const RtcDriverApi *api = device_is_ready(device) ? device->api : NULL;
  return date_time != NULL && api != NULL && api->read_datetime != NULL &&
         api->read_datetime(device, date_time);
}

static bool Read(const struct device *device, RtcDateTime *date_time)
{ (void)device; return RtcStm32ReadDateTime((RtcStm32DateTime *)date_time); }
const RtcDriverApi rtc_stm32_api = {.read_datetime = Read};
int RtcStm32_Init(const struct device *device) { (void)device; return hrtc.Instance == RTC ? 0 : -1; }
