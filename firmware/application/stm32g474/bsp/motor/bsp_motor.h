#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_MOTOR_COMPARE_MAX 8499

typedef enum {
  BSP_MOTOR_LEFT = 0,
  BSP_MOTOR_RIGHT
} BspMotorId;

void BspMotor_Init(void);
bool BspMotor_Start(void);
void BspMotor_SetSignedDuty(BspMotorId motor, int16_t duty);
void BspMotor_SetSignedDutyBoth(int16_t left_duty, int16_t right_duty);
void BspMotor_CoastAll(void);
void BspMotor_EmergencyStop(void);
void BspMotor_ClearEmergencyStop(void);
int16_t BspMotor_GetAppliedDuty(BspMotorId motor);

#endif
