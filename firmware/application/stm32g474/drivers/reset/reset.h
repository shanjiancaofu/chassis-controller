#ifndef RESET_H
#define RESET_H

#include <stdbool.h>
#include <stdint.h>

#define RESET_CAUSE_PIN (1UL << 0)
#define RESET_CAUSE_BOR (1UL << 1)
#define RESET_CAUSE_SOFTWARE (1UL << 2)
#define RESET_CAUSE_IWDG (1UL << 3)
#define RESET_CAUSE_WWDG (1UL << 4)
#define RESET_CAUSE_LOW_POWER (1UL << 5)
#define RESET_CAUSE_OPTION_BYTE (1UL << 6)

bool Reset_WasIndependentWatchdog(void);
uint32_t Reset_GetCauseFlags(void);
void Reset_ClearCauseFlags(void);
uint32_t Reset_ReadBackupRegister(uint32_t index);
void Reset_WriteBackupRegister(uint32_t index, uint32_t value);
void Reset_RequestSystemReset(void) __attribute__((noreturn));

#endif
