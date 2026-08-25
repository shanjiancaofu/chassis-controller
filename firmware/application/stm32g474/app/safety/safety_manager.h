#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <stdbool.h>

typedef enum {
  CHASSIS_CONTROL_STOPPED = 0,
  CHASSIS_CONTROL_RUNNING,
  CHASSIS_CONTROL_COMMAND_TIMEOUT,
  CHASSIS_CONTROL_EMERGENCY_STOP,
  CHASSIS_CONTROL_INTERNAL_FAULT,
  CHASSIS_CONTROL_OPEN_LOOP_TEST,
  CHASSIS_CONTROL_STARTING
} ChassisControlState;

void SafetyManager_Init(bool emergency_stop_active);
void SafetyManager_BeginStartup(void);
void SafetyManager_CompleteStartup(bool emergency_stop_active);
bool SafetyManager_RequestRun(bool command_available);
bool SafetyManager_RequestOpenLoopTest(void);
void SafetyManager_Stop(void);
void SafetyManager_EnterCommandTimeout(void);
void SafetyManager_StopFromEmergencyStopIsr(void);
void SafetyManager_LatchEmergencyStop(void);
bool SafetyManager_ClearEmergencyStop(void);
void SafetyManager_LatchInternalFault(void);
bool SafetyManager_IsRunning(void);
bool SafetyManager_IsOpenLoopTestRunning(void);
bool SafetyManager_IsEmergencyStopLatched(void);
bool SafetyManager_IsStarting(void);
ChassisControlState SafetyManager_GetState(void);

#endif
