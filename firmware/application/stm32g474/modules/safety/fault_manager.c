#include "modules/safety/fault_manager.h"

#define RECOVERABLE_FAULTS \
  (CHASSIS_FAULT_COMMAND_TIMEOUT | CHASSIS_FAULT_EMERGENCY_STOP)
#define CRITICAL_FAULTS \
  (CHASSIS_FAULT_CONTROL_OVERRUN | CHASSIS_FAULT_INTERNAL)
#define KNOWN_FAULTS (RECOVERABLE_FAULTS | CRITICAL_FAULTS)

static uint32_t active_faults;

void FaultManager_Init(void)
{
  __atomic_store_n(&active_faults, CHASSIS_FAULT_NONE, __ATOMIC_RELAXED);
}

void FaultManager_Raise(uint32_t faults)
{
  faults &= KNOWN_FAULTS;
  if (faults == CHASSIS_FAULT_NONE) {
    faults = CHASSIS_FAULT_INTERNAL;
  }
  (void)__atomic_fetch_or(&active_faults, faults, __ATOMIC_RELAXED);
}

void FaultManager_ClearRecoverable(uint32_t faults)
{
  (void)__atomic_fetch_and(&active_faults,
                           ~(faults & RECOVERABLE_FAULTS),
                           __ATOMIC_RELAXED);
}

uint32_t FaultManager_GetFlags(void)
{
  return __atomic_load_n(&active_faults, __ATOMIC_RELAXED);
}

bool FaultManager_HasAny(uint32_t faults)
{
  return (FaultManager_GetFlags() & faults) != 0U;
}

bool FaultManager_HasCritical(void)
{
  return FaultManager_HasAny(CRITICAL_FAULTS);
}
