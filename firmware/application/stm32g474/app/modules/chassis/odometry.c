#include "app/modules/chassis/odometry.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#include "app/modules/chassis/differential_drive.h"

#define PI 3.14159265358979323846f
#define TWO_PI 6.28318530717958647692f
#define STRAIGHT_HEADING_EPSILON_RAD 0.000001f

static OdometryConfig odometry_config;
static OdometrySnapshot odometry_snapshot;
static float distance_per_count_m;
static bool odometry_initialized;

static int64_t SaturatingAdd(int64_t value, int32_t increment);
static float WrapHeading(float heading_rad);

bool Odometry_Init(const OdometryConfig *config)
{
  if (config == NULL || config->encoder_counts_per_revolution == 0U ||
      !isfinite(config->wheel_diameter_m) ||
      !isfinite(config->track_width_m) || config->wheel_diameter_m <= 0.0f ||
      config->track_width_m <= 0.0f) {
    odometry_initialized = false;
    return false;
  }

  odometry_config = *config;
  distance_per_count_m =
      PI * odometry_config.wheel_diameter_m /
      (float)odometry_config.encoder_counts_per_revolution;
  odometry_snapshot = (OdometrySnapshot){0};
  odometry_initialized = true;
  return true;
}

bool Odometry_Update(int32_t left_delta, int32_t right_delta,
                     uint32_t sample_timestamp_ms,
                     uint32_t sample_period_ms)
{
  DifferentialDriveMotionDelta motion_delta;
  float left_delta_m;
  float right_delta_m;
  float heading_before;
  float heading_after;
  float radius_m;
  float period_s;

  if (!odometry_initialized || sample_period_ms == 0U) {
    return false;
  }

  odometry_snapshot.valid = true;
  odometry_snapshot.sample_timestamp_ms = sample_timestamp_ms;
  odometry_snapshot.sample_period_ms = sample_period_ms;
  odometry_snapshot.sample_age_ms = 0U;
  odometry_snapshot.left_delta = left_delta;
  odometry_snapshot.right_delta = right_delta;
  DifferentialDrive_FromWheelDeltas(left_delta, right_delta,
                                    &motion_delta);

  odometry_snapshot.left_total =
      SaturatingAdd(odometry_snapshot.left_total,
                    odometry_snapshot.left_delta);
  odometry_snapshot.right_total =
      SaturatingAdd(odometry_snapshot.right_total,
                    odometry_snapshot.right_delta);
  odometry_snapshot.forward_count_sum_delta =
      motion_delta.forward_count_sum;
  odometry_snapshot.turn_count_difference_delta =
      motion_delta.turn_count_difference;
  odometry_snapshot.forward_count_sum_total =
      SaturatingAdd(odometry_snapshot.forward_count_sum_total,
                    motion_delta.forward_count_sum);
  odometry_snapshot.turn_count_difference_total =
      SaturatingAdd(odometry_snapshot.turn_count_difference_total,
                    motion_delta.turn_count_difference);

  left_delta_m = (float)left_delta * distance_per_count_m;
  right_delta_m = (float)right_delta * distance_per_count_m;
  odometry_snapshot.left_distance_m += left_delta_m;
  odometry_snapshot.right_distance_m += right_delta_m;
  odometry_snapshot.distance_delta_m =
      (left_delta_m + right_delta_m) * 0.5f;
  odometry_snapshot.heading_delta_rad =
      (right_delta_m - left_delta_m) / odometry_config.track_width_m;
  heading_before = odometry_snapshot.heading_rad;
  heading_after = heading_before + odometry_snapshot.heading_delta_rad;
  if (fabsf(odometry_snapshot.heading_delta_rad) <
      STRAIGHT_HEADING_EPSILON_RAD) {
    odometry_snapshot.x_m +=
        odometry_snapshot.distance_delta_m * cosf(heading_before);
    odometry_snapshot.y_m +=
        odometry_snapshot.distance_delta_m * sinf(heading_before);
  } else {
    radius_m = odometry_snapshot.distance_delta_m /
               odometry_snapshot.heading_delta_rad;
    odometry_snapshot.x_m +=
        radius_m * (sinf(heading_after) - sinf(heading_before));
    odometry_snapshot.y_m -=
        radius_m * (cosf(heading_after) - cosf(heading_before));
  }
  odometry_snapshot.heading_rad = WrapHeading(heading_after);
  period_s = (float)sample_period_ms * 0.001f;
  odometry_snapshot.linear_velocity_mps =
      odometry_snapshot.distance_delta_m / period_s;
  odometry_snapshot.angular_velocity_rad_s =
      odometry_snapshot.heading_delta_rad / period_s;
  return true;
}

void Odometry_Reset(void)
{
  odometry_snapshot = (OdometrySnapshot){0};
}

void Odometry_GetSnapshot(uint32_t now_ms, OdometrySnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = odometry_snapshot;
    snapshot->sample_age_ms =
        snapshot->valid ? now_ms - snapshot->sample_timestamp_ms : 0U;
  }
}

static int64_t SaturatingAdd(int64_t value, int32_t increment)
{
  if (increment > 0 && value > INT64_MAX - increment) {
    return INT64_MAX;
  }
  if (increment < 0 && value < INT64_MIN - increment) {
    return INT64_MIN;
  }
  return value + increment;
}

static float WrapHeading(float heading_rad)
{
  while (heading_rad >= PI) {
    heading_rad -= TWO_PI;
  }
  while (heading_rad < -PI) {
    heading_rad += TWO_PI;
  }
  return heading_rad;
}
