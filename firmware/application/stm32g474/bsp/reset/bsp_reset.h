#ifndef BSP_RESET_H
#define BSP_RESET_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_RESET_CAUSE_PIN (1UL << 0)
#define BSP_RESET_CAUSE_BOR (1UL << 1)
#define BSP_RESET_CAUSE_SOFTWARE (1UL << 2)
#define BSP_RESET_CAUSE_IWDG (1UL << 3)
#define BSP_RESET_CAUSE_WWDG (1UL << 4)
#define BSP_RESET_CAUSE_LOW_POWER (1UL << 5)
#define BSP_RESET_CAUSE_OPTION_BYTE (1UL << 6)

bool BspReset_WasIndependentWatchdog(void);
uint32_t BspReset_GetCauseFlags(void);
void BspReset_ClearCauseFlags(void);
uint32_t BspReset_ReadBackupRegister(uint32_t index);
void BspReset_WriteBackupRegister(uint32_t index, uint32_t value);
void BspReset_RequestSystemReset(void) __attribute__((noreturn));

#endif
