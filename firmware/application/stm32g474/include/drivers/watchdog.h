#ifndef CHASSIS_WATCHDOG_H
#define CHASSIS_WATCHDOG_H

#include <stdbool.h>

bool watchdog_refresh(void);
bool watchdog_prepare_for_bootloader(void);

#endif
