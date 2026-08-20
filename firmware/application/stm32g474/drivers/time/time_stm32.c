#include "drivers/time.h"

#include "bsp/time/bsp_time.h"

uint32_t time_uptime_ms(void)
{
  return BspTime_GetUptimeMs();
}
