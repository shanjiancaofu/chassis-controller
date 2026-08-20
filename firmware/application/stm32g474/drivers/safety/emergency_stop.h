#ifndef EMERGENCY_STOP_H
#define EMERGENCY_STOP_H

#include <stdbool.h>

typedef void (*EmergencyStopLatchedCallback)(void);

void EmergencyStop_Init(EmergencyStopLatchedCallback callback);
bool EmergencyStop_IsAsserted(void);
void EmergencyStop_OnInterrupt(void);

#endif
