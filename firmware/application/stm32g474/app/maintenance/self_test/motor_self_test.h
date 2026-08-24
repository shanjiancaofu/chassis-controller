#ifndef MOTOR_SELF_TEST_H
#define MOTOR_SELF_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  MOTOR_SELF_TEST_NONE = 0,
  MOTOR_SELF_TEST_STOP,
  MOTOR_SELF_TEST_LEFT_FORWARD,
  MOTOR_SELF_TEST_LEFT_REVERSE,
  MOTOR_SELF_TEST_RIGHT_FORWARD,
  MOTOR_SELF_TEST_RIGHT_REVERSE
} MotorSelfTestAction;

typedef struct {
  bool running;
  MotorSelfTestAction action;
  int16_t left_duty;
  int16_t right_duty;
} MotorSelfTestSnapshot;

void MotorSelfTest_Init(void);
bool MotorSelfTest_Start(MotorSelfTestAction action, uint32_t now_ms);
void MotorSelfTest_Run(uint32_t now_ms);
void MotorSelfTest_Stop(void);
bool MotorSelfTest_SetDuty(uint16_t duty);
uint16_t MotorSelfTest_GetDuty(void);
void MotorSelfTest_GetSnapshot(MotorSelfTestSnapshot *snapshot);

#endif
