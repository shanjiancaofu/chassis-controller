#ifndef CHASSIS_APP_H
#define CHASSIS_APP_H

#include <stdbool.h>
#include <stdint.h>

void ChassisApp_Init(void);
void ChassisApp_RunServiceCycle(void);
void ChassisApp_RunDiagnosticsCycle(void);
void ChassisApp_RunDisplayCycle(void);
void ChassisApp_RunControlCycle(uint32_t notification_count);
void ChassisApp_FatalError(void);
bool ChassisApp_ClearEmergencyStop(void);

#endif
