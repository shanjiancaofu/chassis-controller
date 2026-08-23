#include "drivers/watchdog/watchdog_stm32_private.h"

#include "iwdg.h"

bool WatchdogStm32Refresh(void)
{
  return HAL_IWDG_Refresh(&hiwdg) == HAL_OK;
}

bool WatchdogStm32PrepareForBootloader(void)
{
  return AppWatchdog_PrepareForBootloader();
}
