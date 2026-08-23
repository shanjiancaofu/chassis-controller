#ifndef CHASSIS_WATCHDOG_STM32_PRIVATE_H
#define CHASSIS_WATCHDOG_STM32_PRIVATE_H

#include <stdbool.h>

bool WatchdogStm32Refresh(void);
bool WatchdogStm32PrepareForBootloader(void);

#endif
