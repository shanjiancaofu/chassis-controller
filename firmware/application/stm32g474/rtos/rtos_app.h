#ifndef RTOS_APP_H
#define RTOS_APP_H

#include <stdbool.h>

bool RtosApp_CreateControlTask(void);
void RtosApp_Run(void);
void RtosApp_NotifyControlFromIsr(void);
bool RtosApp_AreCriticalTasksHealthy(void);

#endif
