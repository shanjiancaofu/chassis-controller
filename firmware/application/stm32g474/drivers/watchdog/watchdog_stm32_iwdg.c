#include "drivers/watchdog/watchdog_stm32_private.h"

#include "boards/chassis_g474/board_config.h"

bool WatchdogStm32Refresh(void)
{
  return HAL_IWDG_Refresh(&BOARD_WATCHDOG) == HAL_OK;
}

bool WatchdogStm32PrepareForBootloader(void)
{
  return AppWatchdog_PrepareForBootloader();
}
