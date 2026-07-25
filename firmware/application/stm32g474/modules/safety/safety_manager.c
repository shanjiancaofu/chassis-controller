#include "modules/safety/safety_manager.h"

#include "modules/safety/fault_manager.h"

static bool emergency_stop_latched;
static ChassisControlState requested_state;

void SafetyManager_Init(bool emergency_stop_active)
{
  __atomic_store_n(&emergency_stop_latched, emergency_stop_active,
                   __ATOMIC_RELAXED);
  requested_state = CHASSIS_CONTROL_STOPPED;
  if (emergency_stop_active) {
    FaultManager_Raise(CHASSIS_FAULT_EMERGENCY_STOP);
  }
}

bool SafetyManager_RequestRun(bool command_available)
{
  if (!command_available || SafetyManager_IsEmergencyStopLatched() ||
      FaultManager_HasCritical() ||
      requested_state == CHASSIS_CONTROL_OPEN_LOOP_TEST) {
    return false;
  }

  FaultManager_ClearRecoverable(CHASSIS_FAULT_COMMAND_TIMEOUT);
  requested_state = CHASSIS_CONTROL_RUNNING;
  return true;
}

bool SafetyManager_RequestOpenLoopTest(void)
{
  if (SafetyManager_IsEmergencyStopLatched() ||
      FaultManager_HasCritical() ||
      requested_state == CHASSIS_CONTROL_RUNNING ||
      requested_state == CHASSIS_CONTROL_OPEN_LOOP_TEST) {
    return false;
  }

  requested_state = CHASSIS_CONTROL_OPEN_LOOP_TEST;
  return true;
}

void SafetyManager_Stop(void)
{
  requested_state = CHASSIS_CONTROL_STOPPED;
}

void SafetyManager_EnterCommandTimeout(void)
{
  FaultManager_Raise(CHASSIS_FAULT_COMMAND_TIMEOUT);
  requested_state = CHASSIS_CONTROL_COMMAND_TIMEOUT;
}

void SafetyManager_LatchEmergencyStopFromIsr(void)
{
  __atomic_store_n(&emergency_stop_latched, true, __ATOMIC_RELAXED);
  FaultManager_Raise(CHASSIS_FAULT_EMERGENCY_STOP);
}

bool SafetyManager_ClearEmergencyStop(void)
{
  if (!SafetyManager_IsEmergencyStopLatched()) {
    return true;
  }
  __atomic_store_n(&emergency_stop_latched, false, __ATOMIC_RELAXED);
  FaultManager_ClearRecoverable(CHASSIS_FAULT_EMERGENCY_STOP);
  SafetyManager_Stop();
  return true;
}

void SafetyManager_LatchInternalFault(void)
{
  requested_state = CHASSIS_CONTROL_INTERNAL_FAULT;
}

bool SafetyManager_IsRunning(void)
{
  return SafetyManager_GetState() == CHASSIS_CONTROL_RUNNING;
}

bool SafetyManager_IsOpenLoopTestRunning(void)
{
  return SafetyManager_GetState() == CHASSIS_CONTROL_OPEN_LOOP_TEST;
}

bool SafetyManager_IsEmergencyStopLatched(void)
{
  return __atomic_load_n(&emergency_stop_latched, __ATOMIC_RELAXED);
}

ChassisControlState SafetyManager_GetState(void)
{
  if (SafetyManager_IsEmergencyStopLatched()) {
    return CHASSIS_CONTROL_EMERGENCY_STOP;
  }
  if (FaultManager_HasCritical()) {
    return CHASSIS_CONTROL_INTERNAL_FAULT;
  }
  return requested_state;
}
