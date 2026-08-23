#include "app/modules/safety/safety_manager.h"

#include "app/modules/safety/fault_manager.h"

static bool emergency_stop_latched;
static ChassisControlState requested_state;

void SafetyManager_Init(bool emergency_stop_active)
{
  __atomic_store_n(&emergency_stop_latched, emergency_stop_active,
                   __ATOMIC_RELAXED);
  __atomic_store_n(&requested_state, CHASSIS_CONTROL_STOPPED,
                   __ATOMIC_RELAXED);
  if (emergency_stop_active) {
    FaultManager_Raise(CHASSIS_FAULT_EMERGENCY_STOP);
  }
}

bool SafetyManager_RequestRun(bool command_available)
{
  if (!command_available || SafetyManager_IsEmergencyStopLatched() ||
      FaultManager_HasCritical() ||
      __atomic_load_n(&requested_state, __ATOMIC_RELAXED) ==
          CHASSIS_CONTROL_OPEN_LOOP_TEST) {
    return false;
  }

  FaultManager_ClearRecoverable(CHASSIS_FAULT_COMMAND_TIMEOUT);
  __atomic_store_n(&requested_state, CHASSIS_CONTROL_RUNNING,
                   __ATOMIC_RELAXED);
  return true;
}

bool SafetyManager_RequestOpenLoopTest(void)
{
  const ChassisControlState state =
      __atomic_load_n(&requested_state, __ATOMIC_RELAXED);

  if (SafetyManager_IsEmergencyStopLatched() ||
      FaultManager_HasCritical() ||
      state == CHASSIS_CONTROL_RUNNING ||
      state == CHASSIS_CONTROL_OPEN_LOOP_TEST) {
    return false;
  }

  __atomic_store_n(&requested_state, CHASSIS_CONTROL_OPEN_LOOP_TEST,
                   __ATOMIC_RELAXED);
  return true;
}

void SafetyManager_Stop(void)
{
  __atomic_store_n(&requested_state, CHASSIS_CONTROL_STOPPED,
                   __ATOMIC_RELAXED);
}

void SafetyManager_EnterCommandTimeout(void)
{
  FaultManager_Raise(CHASSIS_FAULT_COMMAND_TIMEOUT);
  __atomic_store_n(&requested_state, CHASSIS_CONTROL_COMMAND_TIMEOUT,
                   __ATOMIC_RELAXED);
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
  __atomic_store_n(&requested_state, CHASSIS_CONTROL_INTERNAL_FAULT,
                   __ATOMIC_RELAXED);
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
  return __atomic_load_n(&requested_state, __ATOMIC_RELAXED);
}
