#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

#include <stdbool.h>

bool BspWatchdog_Refresh(void);
bool BspWatchdog_PrepareForBootloader(void);

#endif
