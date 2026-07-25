#include "tests/target/motor_target_test.h"

#include <stddef.h>

#include "bsp/motor/bsp_motor.h"
#include "config/target_test_config.h"
#include "modules/safety/safety_manager.h"

static MotorTargetTestSnapshot test_snapshot;
static uint32_t test_started_ms;

void MotorTargetTest_Init(void)
{
  test_snapshot = (MotorTargetTestSnapshot){0};
  test_started_ms = 0U;
}

bool MotorTargetTest_Start(MotorTargetTestAction action, uint32_t now_ms)
{
  int16_t left_duty = 0;
  int16_t right_duty = 0;

  if (action == MOTOR_TARGET_TEST_STOP) {
    MotorTargetTest_Stop();
    return true;
  }
  if (test_snapshot.running ||
      !SafetyManager_IsOpenLoopTestRunning()) {
    return false;
  }

  switch (action) {
    case MOTOR_TARGET_TEST_LEFT_FORWARD:
      left_duty = MOTOR_OPEN_LOOP_TEST_DUTY;
      break;
    case MOTOR_TARGET_TEST_LEFT_REVERSE:
      left_duty = -MOTOR_OPEN_LOOP_TEST_DUTY;
      break;
    case MOTOR_TARGET_TEST_RIGHT_FORWARD:
      right_duty = MOTOR_OPEN_LOOP_TEST_DUTY;
      break;
    case MOTOR_TARGET_TEST_RIGHT_REVERSE:
      right_duty = -MOTOR_OPEN_LOOP_TEST_DUTY;
      break;
    default:
      return false;
  }

  BspMotor_CoastAll();
  BspMotor_SetSignedDutyBoth(left_duty, right_duty);
  test_snapshot.running = true;
  test_snapshot.action = action;
  test_snapshot.left_duty = left_duty;
  test_snapshot.right_duty = right_duty;
  test_started_ms = now_ms;
  return true;
}

void MotorTargetTest_Run(uint32_t now_ms)
{
  if (!test_snapshot.running) {
    return;
  }
  if (!SafetyManager_IsOpenLoopTestRunning()) {
    BspMotor_CoastAll();
    test_snapshot = (MotorTargetTestSnapshot){0};
    return;
  }
  if (now_ms - test_started_ms >= MOTOR_OPEN_LOOP_TEST_DURATION_MS) {
    MotorTargetTest_Stop();
  }
}

void MotorTargetTest_Stop(void)
{
  BspMotor_CoastAll();
  test_snapshot = (MotorTargetTestSnapshot){0};
  if (SafetyManager_IsOpenLoopTestRunning()) {
    SafetyManager_Stop();
  }
}

void MotorTargetTest_GetSnapshot(MotorTargetTestSnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = test_snapshot;
  }
}
