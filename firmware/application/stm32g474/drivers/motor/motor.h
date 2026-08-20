#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_COMPARE_MAX 8499

typedef enum {
  MOTOR_LEFT = 0,
  MOTOR_RIGHT
} MotorId;

void Motor_Init(void);
bool Motor_Start(void);
void Motor_SetSignedDuty(MotorId motor, int16_t duty);
void Motor_SetSignedDutyBoth(int16_t left_duty, int16_t right_duty);
void Motor_CoastAll(void);
void Motor_EmergencyStop(void);
void Motor_ClearEmergencyStop(void);
int16_t Motor_GetAppliedDuty(MotorId motor);

#endif
