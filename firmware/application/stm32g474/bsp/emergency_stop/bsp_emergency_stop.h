#ifndef BSP_EMERGENCY_STOP_H
#define BSP_EMERGENCY_STOP_H

#include <stdbool.h>

typedef void (*BspEmergencyStopLatchedCallback)(void);

void BspEmergencyStop_Init(BspEmergencyStopLatchedCallback callback);
bool BspEmergencyStop_IsAsserted(void);
void BspEmergencyStop_OnInterrupt(void);

#endif
