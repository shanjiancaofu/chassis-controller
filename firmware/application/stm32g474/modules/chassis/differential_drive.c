#include "modules/chassis/differential_drive.h"

#include <limits.h>
#include <stddef.h>

void DifferentialDrive_FromWheelDeltas(
    int32_t left_delta, int32_t right_delta,
    DifferentialDriveMotionDelta *motion_delta)
{
  int64_t forward;
  int64_t turn;

  if (motion_delta == NULL) {
    return;
  }

  forward = (int64_t)left_delta + right_delta;
  turn = (int64_t)right_delta - left_delta;
  motion_delta->forward_count_sum =
      forward > INT32_MAX ? INT32_MAX
                          : forward < INT32_MIN ? INT32_MIN
                                                : (int32_t)forward;
  motion_delta->turn_count_difference =
      turn > INT32_MAX ? INT32_MAX
                       : turn < INT32_MIN ? INT32_MIN : (int32_t)turn;
}

bool DifferentialDrive_ToWheelTargets(int32_t forward_component,
                                      int32_t turn_component,
                                      int32_t target_limit,
                                      int32_t *left_target,
                                      int32_t *right_target)
{
  const int64_t left = (int64_t)forward_component - turn_component;
  const int64_t right = (int64_t)forward_component + turn_component;

  if (left_target == NULL || right_target == NULL || target_limit <= 0 ||
      left < -target_limit || left > target_limit ||
      right < -target_limit || right > target_limit ||
      left < INT32_MIN || left > INT32_MAX ||
      right < INT32_MIN || right > INT32_MAX) {
    return false;
  }

  *left_target = (int32_t)left;
  *right_target = (int32_t)right;
  return true;
}
