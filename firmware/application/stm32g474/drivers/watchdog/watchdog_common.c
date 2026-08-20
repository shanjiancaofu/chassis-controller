#include "drivers/watchdog.h"

#include "drivers/watchdog/watchdog_stm32_private.h"
#include "device.h"
#include "devicetree_generated.h"

bool watchdog_refresh(void)
{
  return device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_WATCHDOG)) &&
         WatchdogStm32Refresh();
}

bool watchdog_prepare_for_bootloader(void)
{
  return device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_WATCHDOG)) &&
         WatchdogStm32PrepareForBootloader();
}
