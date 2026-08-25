#ifdef SAFETY_MANAGER_HOST_TEST

#include <assert.h>

#include "app/safety/fault_manager.h"
#include "app/safety/safety_manager.h"

int main(void)
{
  FaultManager_Init();
  SafetyManager_BeginStartup();
  assert(SafetyManager_IsStarting());
  assert(SafetyManager_GetState() == CHASSIS_CONTROL_STARTING);
  assert(!SafetyManager_RequestRun(true));
  assert(!SafetyManager_RequestOpenLoopTest());

  SafetyManager_StopFromEmergencyStopIsr();
  assert(!SafetyManager_IsEmergencyStopLatched());
  assert(FaultManager_GetFlags() == CHASSIS_FAULT_NONE);
  SafetyManager_CompleteStartup(false);
  assert(!SafetyManager_IsStarting());
  assert(!SafetyManager_IsEmergencyStopLatched());
  assert(SafetyManager_GetState() == CHASSIS_CONTROL_STOPPED);
  assert(FaultManager_GetFlags() == CHASSIS_FAULT_NONE);

  SafetyManager_BeginStartup();
  SafetyManager_CompleteStartup(true);
  assert(SafetyManager_GetState() == CHASSIS_CONTROL_EMERGENCY_STOP);
  assert(FaultManager_HasAny(CHASSIS_FAULT_EMERGENCY_STOP));

  FaultManager_Init();
  SafetyManager_Init(false);
  assert(SafetyManager_RequestRun(true));
  SafetyManager_StopFromEmergencyStopIsr();
  assert(SafetyManager_GetState() == CHASSIS_CONTROL_STOPPED);
  assert(!SafetyManager_IsEmergencyStopLatched());
  assert(FaultManager_GetFlags() == CHASSIS_FAULT_NONE);
  SafetyManager_LatchEmergencyStop();
  assert(SafetyManager_GetState() == CHASSIS_CONTROL_EMERGENCY_STOP);
  return 0;
}

#endif
