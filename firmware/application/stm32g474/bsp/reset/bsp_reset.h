#ifndef BSP_RESET_H
#define BSP_RESET_H

#include <stdbool.h>
#include <stdint.h>

bool BspReset_WasIndependentWatchdog(void);
void BspReset_ClearCauseFlags(void);
uint32_t BspReset_ReadBackupRegister(uint32_t index);
void BspReset_WriteBackupRegister(uint32_t index, uint32_t value);

#endif
