#include "drivers/time.h"

#include "drivers/time/time_stm32_private.h"
#include "device.h"
#include "devicetree_generated.h"

uint32_t time_uptime_ms(void)
{
  return device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_TIME))
             ? TimeStm32GetUptimeMs()
             : 0U;
}
