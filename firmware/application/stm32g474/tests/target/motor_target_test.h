#ifndef MOTOR_TARGET_TEST_H
#define MOTOR_TARGET_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  MOTOR_TARGET_TEST_NONE = 0,
  MOTOR_TARGET_TEST_STOP,
  MOTOR_TARGET_TEST_LEFT_FORWARD,
  MOTOR_TARGET_TEST_LEFT_REVERSE,
  MOTOR_TARGET_TEST_RIGHT_FORWARD,
  MOTOR_TARGET_TEST_RIGHT_REVERSE
} MotorTargetTestAction;

typedef struct {
  bool running;
  MotorTargetTestAction action;
  int16_t left_duty;
  int16_t right_duty;
} MotorTargetTestSnapshot;

void MotorTargetTest_Init(void);
bool MotorTargetTest_Start(MotorTargetTestAction action, uint32_t now_ms);
void MotorTargetTest_Run(uint32_t now_ms);
void MotorTargetTest_Stop(void);
bool MotorTargetTest_SetDuty(uint16_t duty);
uint16_t MotorTargetTest_GetDuty(void);
void MotorTargetTest_GetSnapshot(MotorTargetTestSnapshot *snapshot);

#endif
