#include "app/safety/fault_manager.h"

#define RECOVERABLE_FAULTS \
  (CHASSIS_FAULT_COMMAND_TIMEOUT | CHASSIS_FAULT_EMERGENCY_STOP)
#define CRITICAL_FAULTS \
  (CHASSIS_FAULT_CONTROL_OVERRUN | CHASSIS_FAULT_INTERNAL | \
   CHASSIS_FAULT_ENCODER | CHASSIS_FAULT_UNDERVOLTAGE)
#define KNOWN_FAULTS (RECOVERABLE_FAULTS | CRITICAL_FAULTS)

static uint32_t active_faults;
static uint32_t latched_faults;
static uint32_t fault_sequence;

void FaultManager_Init(void)
{
  __atomic_store_n(&active_faults, CHASSIS_FAULT_NONE, __ATOMIC_RELAXED);
  __atomic_store_n(&latched_faults, CHASSIS_FAULT_NONE, __ATOMIC_RELAXED);
  __atomic_store_n(&fault_sequence, 0U, __ATOMIC_RELAXED);
}

void FaultManager_Raise(uint32_t faults)
{
  uint32_t previous;

  faults &= KNOWN_FAULTS;
  if (faults == CHASSIS_FAULT_NONE) {
    faults = CHASSIS_FAULT_INTERNAL;
  }
  previous = __atomic_fetch_or(&active_faults, faults, __ATOMIC_RELAXED);
  (void)__atomic_fetch_or(&latched_faults, faults, __ATOMIC_RELAXED);
  if ((previous & faults) != faults) {
    (void)__atomic_fetch_add(&fault_sequence, 1U, __ATOMIC_RELAXED);
  }
}

void FaultManager_ClearRecoverable(uint32_t faults)
{
  const uint32_t clear_mask = faults & RECOVERABLE_FAULTS;
  const uint32_t previous = __atomic_fetch_and(
      &active_faults, ~clear_mask, __ATOMIC_RELAXED);

  if ((previous & clear_mask) != 0U) {
    (void)__atomic_fetch_add(&fault_sequence, 1U, __ATOMIC_RELAXED);
  }
}

uint32_t FaultManager_GetFlags(void)
{
  return __atomic_load_n(&active_faults, __ATOMIC_RELAXED);
}

uint32_t FaultManager_GetLatchedFlags(void)
{
  return __atomic_load_n(&latched_faults, __ATOMIC_RELAXED);
}

uint16_t FaultManager_GetSequence(void)
{
  return (uint16_t)__atomic_load_n(&fault_sequence, __ATOMIC_RELAXED);
}

bool FaultManager_HasAny(uint32_t faults)
{
  return (FaultManager_GetFlags() & faults) != 0U;
}

bool FaultManager_HasCritical(void)
{
  return FaultManager_HasAny(CRITICAL_FAULTS);
}
