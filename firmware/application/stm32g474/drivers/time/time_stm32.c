#include "drivers/time/time_stm32_private.h"

#include "stm32g4xx_hal.h"

uint32_t TimeStm32GetUptimeMs(void)
{
  return HAL_GetTick();
}
