#include "bsp/time/bsp_time.h"

#include "stm32g4xx_hal.h"

uint32_t BspTime_GetUptimeMs(void)
{
  return HAL_GetTick();
}
