#include "app/chassis/differential_drive.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#define PI_F 3.14159265358979323846f

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

bool DifferentialDrive_VelocityToWheelTargets(
    int32_t linear_velocity_mm_s, int32_t angular_velocity_mrad_s,
    uint32_t encoder_counts_per_revolution, float wheel_diameter_m,
    float track_width_m, uint32_t control_period_ms, int32_t target_limit,
    int32_t *left_target, int32_t *right_target)
{
  const float linear_velocity_m_s = linear_velocity_mm_s / 1000.0f;
  const float angular_velocity_rad_s = angular_velocity_mrad_s / 1000.0f;
  const float left_velocity_m_s =
      linear_velocity_m_s - angular_velocity_rad_s * track_width_m * 0.5f;
  const float right_velocity_m_s =
      linear_velocity_m_s + angular_velocity_rad_s * track_width_m * 0.5f;
  const float circumference_m = PI_F * wheel_diameter_m;
  float left_counts;
  float right_counts;
  long rounded_left;
  long rounded_right;

  if (left_target == NULL || right_target == NULL ||
      encoder_counts_per_revolution == 0U || wheel_diameter_m <= 0.0f ||
      track_width_m <= 0.0f || control_period_ms == 0U ||
      target_limit <= 0 || !isfinite(left_velocity_m_s) ||
      !isfinite(right_velocity_m_s) || !isfinite(circumference_m)) {
    return false;
  }
  left_counts = left_velocity_m_s * encoder_counts_per_revolution *
                control_period_ms / (circumference_m * 1000.0f);
  right_counts = right_velocity_m_s * encoder_counts_per_revolution *
                 control_period_ms / (circumference_m * 1000.0f);
  if (!isfinite(left_counts) || !isfinite(right_counts) ||
      left_counts < -target_limit || left_counts > target_limit ||
      right_counts < -target_limit || right_counts > target_limit) {
    return false;
  }
  rounded_left = lroundf(left_counts);
  rounded_right = lroundf(right_counts);
  if (rounded_left < -target_limit || rounded_left > target_limit ||
      rounded_right < -target_limit || rounded_right > target_limit ||
      rounded_left < INT32_MIN || rounded_left > INT32_MAX ||
      rounded_right < INT32_MIN || rounded_right > INT32_MAX) {
    return false;
  }
  *left_target = (int32_t)rounded_left;
  *right_target = (int32_t)rounded_right;
  return true;
}
