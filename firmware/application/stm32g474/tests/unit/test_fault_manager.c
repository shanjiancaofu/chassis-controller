#ifdef FAULT_MANAGER_HOST_TEST

#include <assert.h>

#include "app/safety/fault_manager.h"

int main(void)
{
  FaultManager_Init();
  assert(FaultManager_GetFlags() == CHASSIS_FAULT_NONE);
  assert(FaultManager_GetLatchedFlags() == CHASSIS_FAULT_NONE);
  assert(FaultManager_GetSequence() == 0U);

  FaultManager_Raise(CHASSIS_FAULT_COMMAND_TIMEOUT);
  assert(FaultManager_GetFlags() == CHASSIS_FAULT_COMMAND_TIMEOUT);
  assert(FaultManager_GetLatchedFlags() == CHASSIS_FAULT_COMMAND_TIMEOUT);
  assert(FaultManager_GetSequence() == 1U);

  FaultManager_Raise(CHASSIS_FAULT_COMMAND_TIMEOUT);
  assert(FaultManager_GetSequence() == 1U);
  FaultManager_ClearRecoverable(CHASSIS_FAULT_COMMAND_TIMEOUT);
  assert(FaultManager_GetFlags() == CHASSIS_FAULT_NONE);
  assert(FaultManager_GetLatchedFlags() == CHASSIS_FAULT_COMMAND_TIMEOUT);
  assert(FaultManager_GetSequence() == 2U);

  FaultManager_Raise(CHASSIS_FAULT_UNDERVOLTAGE);
  assert(FaultManager_HasCritical());
  assert(FaultManager_GetLatchedFlags() ==
         (CHASSIS_FAULT_COMMAND_TIMEOUT | CHASSIS_FAULT_UNDERVOLTAGE));
  assert(FaultManager_GetSequence() == 3U);
  return 0;
}

#endif
