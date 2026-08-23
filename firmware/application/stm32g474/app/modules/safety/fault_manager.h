#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

enum {
  CHASSIS_FAULT_NONE = 0U,
  CHASSIS_FAULT_COMMAND_TIMEOUT = 1U << 0,
  CHASSIS_FAULT_EMERGENCY_STOP = 1U << 1,
  CHASSIS_FAULT_CONTROL_OVERRUN = 1U << 2,
  CHASSIS_FAULT_INTERNAL = 1U << 3,
  CHASSIS_FAULT_ENCODER = 1U << 4,
  CHASSIS_FAULT_UNDERVOLTAGE = 1U << 5
};

void FaultManager_Init(void);
void FaultManager_Raise(uint32_t faults);
void FaultManager_ClearRecoverable(uint32_t faults);
uint32_t FaultManager_GetFlags(void);
bool FaultManager_HasAny(uint32_t faults);
bool FaultManager_HasCritical(void);

#endif
