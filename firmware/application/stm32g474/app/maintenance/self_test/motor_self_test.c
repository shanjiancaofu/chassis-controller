#include "app/maintenance/self_test/motor_self_test.h"
#include "device.h"
#include "devicetree.h"
#include "drivers/motor/motor.h"

#include <stddef.h>

#include "config/self_test_config.h"
#include "app/safety/safety_manager.h"

static MotorSelfTestSnapshot test_snapshot;
static uint32_t test_started_ms;
static uint16_t test_duty;

void MotorSelfTest_Init(void)
{
  test_snapshot = (MotorSelfTestSnapshot){0};
  test_started_ms = 0U;
  test_duty = MOTOR_OPEN_LOOP_TEST_DUTY;
}

bool MotorSelfTest_Start(MotorSelfTestAction action, uint32_t now_ms)
{
  int16_t left_duty = 0;
  int16_t right_duty = 0;

  if (action == MOTOR_SELF_TEST_STOP) {
    MotorSelfTest_Stop();
    return true;
  }
  if (test_snapshot.running ||
      !SafetyManager_IsOpenLoopTestRunning()) {
    return false;
  }

  switch (action) {
    case MOTOR_SELF_TEST_LEFT_FORWARD:
      left_duty = (int16_t)test_duty;
      break;
    case MOTOR_SELF_TEST_LEFT_REVERSE:
      left_duty = -(int16_t)test_duty;
      break;
    case MOTOR_SELF_TEST_RIGHT_FORWARD:
      right_duty = (int16_t)test_duty;
      break;
    case MOTOR_SELF_TEST_RIGHT_REVERSE:
      right_duty = -(int16_t)test_duty;
      break;
    default:
      return false;
  }

  motor_coast_all(DEVICE_DT_GET(DT_NODELABEL(drive0)));
  motor_set_signed_duty_both(DEVICE_DT_GET(DT_NODELABEL(drive0)), left_duty, right_duty);
  test_snapshot.running = true;
  test_snapshot.action = action;
  test_snapshot.left_duty = left_duty;
  test_snapshot.right_duty = right_duty;
  test_started_ms = now_ms;
  return true;
}

void MotorSelfTest_Run(uint32_t now_ms)
{
  if (!test_snapshot.running) {
    return;
  }
  if (!SafetyManager_IsOpenLoopTestRunning()) {
    motor_coast_all(DEVICE_DT_GET(DT_NODELABEL(drive0)));
    test_snapshot = (MotorSelfTestSnapshot){0};
    return;
  }
  if (now_ms - test_started_ms >= MOTOR_OPEN_LOOP_TEST_DURATION_MS) {
    MotorSelfTest_Stop();
  }
}

void MotorSelfTest_Stop(void)
{
  motor_coast_all(DEVICE_DT_GET(DT_NODELABEL(drive0)));
  test_snapshot = (MotorSelfTestSnapshot){0};
  if (SafetyManager_IsOpenLoopTestRunning()) {
    SafetyManager_Stop();
  }
}

bool MotorSelfTest_SetDuty(uint16_t duty)
{
  if (test_snapshot.running || duty > MOTOR_OPEN_LOOP_TEST_DUTY_MAX) {
    return false;
  }
  test_duty = duty;
  return true;
}

uint16_t MotorSelfTest_GetDuty(void)
{
  return test_duty;
}

void MotorSelfTest_GetSnapshot(MotorSelfTestSnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = test_snapshot;
  }
}
