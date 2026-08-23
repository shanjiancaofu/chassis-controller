#include "drivers/time.h"

#include "drivers/time/time_stm32_private.h"
#include "device.h"
#include "devicetree.h"

uint32_t time_uptime_ms(void)
{
  const struct device *device = DEVICE_DT_GET(DT_CHOSEN(chassis_time));
  const TimeDriverApi *api = device_is_ready(device) ? device->api : NULL;
  return api != NULL && api->uptime_ms != NULL ? api->uptime_ms(device) : 0U;
}

static uint32_t Uptime(const struct device *device) { (void)device; return TimeStm32GetUptimeMs(); }
const TimeDriverApi time_stm32_api = {.uptime_ms = Uptime};
int TimeStm32_Init(const struct device *device) { (void)device; return 0; }
