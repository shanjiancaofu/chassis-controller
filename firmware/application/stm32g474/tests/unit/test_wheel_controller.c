#ifdef WHEEL_CONTROLLER_HOST_TEST

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/chassis/wheel_controller.h"

static int16_t applied_left;
static int16_t applied_right;
static uint32_t apply_count;
static uint32_t coast_count;
static uint32_t emergency_stop_count;

static void FakeCoastAll(void)
{
  ++coast_count;
  applied_left = 0;
  applied_right = 0;
}

static void FakeEmergencyStop(void)
{
  ++emergency_stop_count;
  applied_left = 0;
  applied_right = 0;
}

static void FakeSetSignedDutyBoth(int16_t left_duty, int16_t right_duty)
{
  ++apply_count;
  applied_left = left_duty;
  applied_right = right_duty;
}

static int16_t FakeGetLeftAppliedDuty(void)
{
  return applied_left;
}

static int16_t FakeGetRightAppliedDuty(void)
{
  return applied_right;
}

int main(void)
{
  const WheelControllerMotorPort motor_port = {
      .coast_all = FakeCoastAll,
      .emergency_stop = FakeEmergencyStop,
      .set_signed_duty_both = FakeSetSignedDutyBoth,
      .get_left_applied_duty = FakeGetLeftAppliedDuty,
      .get_right_applied_duty = FakeGetRightAppliedDuty,
  };
  WheelControllerSnapshot snapshot;

  assert(!WheelController_Init(NULL));
  assert(!WheelController_Update(1, 1, 0, 0, 1U));
  assert(WheelController_Init(&motor_port));
  assert(!WheelController_Update(1, 1, 0, 0, 0U));
  assert(!WheelController_Update(101, 0, 0, 0, 1U));
  assert(WheelController_Update(5, -5, 0, 0, 1U));
  assert(apply_count == 1U);
  assert(applied_left > 0);
  assert(applied_right < 0);

  WheelController_GetSnapshot(&snapshot);
  assert(snapshot.left_target == 5);
  assert(snapshot.right_target == -5);
  assert(snapshot.left_output == applied_left);
  assert(snapshot.right_output == applied_right);

  WheelController_Stop();
  assert(coast_count == 1U);
  WheelController_GetSnapshot(&snapshot);
  assert(snapshot.left_target == 0);
  assert(snapshot.right_target == 0);
  assert(snapshot.left_output == 0);
  assert(snapshot.right_output == 0);

  assert(WheelController_Update(5, 5, 0, 0, 1U));
  WheelController_EmergencyStop();
  assert(emergency_stop_count == 1U);
  assert(applied_left == 0);
  assert(applied_right == 0);
  WheelController_GetSnapshot(&snapshot);
  assert(snapshot.left_output == 0);
  assert(snapshot.right_output == 0);

  WheelController_Stop();
  WheelController_ApplyPidGains(WHEEL_CONTROLLER_LEFT, 0U, 100U, 0U);
  WheelController_ApplyPidGains(WHEEL_CONTROLLER_RIGHT, 0U, 100U, 0U);
  assert(WheelController_Update(5, 5, 0, 0, 1U));
  assert(applied_left == 5 && applied_right == 5);
  WheelController_Stop();
  assert(WheelController_Update(5, 5, 0, 0, 3U));
  assert(applied_left == 15 && applied_right == 15);
  return 0;
}

#endif
