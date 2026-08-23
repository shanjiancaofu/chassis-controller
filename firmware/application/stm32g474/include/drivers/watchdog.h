#ifndef CHASSIS_WATCHDOG_H
#define CHASSIS_WATCHDOG_H

#include <stdbool.h>
#include "device.h"

typedef struct {
  bool (*refresh)(const struct device *device);
  bool (*prepare_for_bootloader)(const struct device *device);
} WatchdogDriverApi;

bool watchdog_refresh(void);
bool watchdog_prepare_for_bootloader(void);
extern const WatchdogDriverApi watchdog_stm32_api;
int WatchdogStm32_Init(const struct device *device);

#endif
