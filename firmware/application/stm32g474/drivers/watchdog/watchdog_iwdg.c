#include "drivers/watchdog.h"

#include "bsp/watchdog/bsp_watchdog.h"

bool watchdog_refresh(void)
{
  return BspWatchdog_Refresh();
}

bool watchdog_prepare_for_bootloader(void)
{
  return BspWatchdog_PrepareForBootloader();
}
