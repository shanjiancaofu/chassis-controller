#include "bsp/watchdog/bsp_watchdog.h"

#include "board/board_config.h"

bool BspWatchdog_Refresh(void)
{
  return HAL_IWDG_Refresh(&BOARD_WATCHDOG) == HAL_OK;
}

bool BspWatchdog_PrepareForBootloader(void)
{
  return AppWatchdog_PrepareForBootloader();
}
