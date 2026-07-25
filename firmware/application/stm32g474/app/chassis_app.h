#ifndef CHASSIS_APP_H
#define CHASSIS_APP_H

void ChassisApp_Init(void);
void ChassisApp_Run(void);
void ChassisApp_ControlTickFromIsr(void);
void ChassisApp_FatalError(void);

#endif
