#include "drivers/watchdog.h"

#include "drivers/watchdog/watchdog_stm32_private.h"
#include "device.h"
#include "devicetree.h"
#include "iwdg.h"

bool watchdog_refresh(void)
{
  const struct device *device = DEVICE_DT_GET(DT_CHOSEN(chassis_watchdog));
  const WatchdogDriverApi *api = device_is_ready(device) ? device->api : NULL;
  return api != NULL && api->refresh != NULL && api->refresh(device);
}

bool watchdog_prepare_for_bootloader(void)
{
  const struct device *device = DEVICE_DT_GET(DT_CHOSEN(chassis_watchdog));
  const WatchdogDriverApi *api = device_is_ready(device) ? device->api : NULL;
  return api != NULL && api->prepare_for_bootloader != NULL &&
         api->prepare_for_bootloader(device);
}

static bool Refresh(const struct device *device) { (void)device; return WatchdogStm32Refresh(); }
static bool Prepare(const struct device *device) { (void)device; return WatchdogStm32PrepareForBootloader(); }
const WatchdogDriverApi watchdog_stm32_api = {.refresh = Refresh, .prepare_for_bootloader = Prepare};
int WatchdogStm32_Init(const struct device *device) { (void)device; return hiwdg.Instance == IWDG ? 0 : -1; }
