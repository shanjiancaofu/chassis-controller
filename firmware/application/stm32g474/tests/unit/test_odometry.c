#ifdef ODOMETRY_HOST_TEST

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "app/chassis/differential_drive.h"
#include "app/chassis/odometry.h"

#define PI_F 3.14159265358979323846f

static bool NearlyEqual(float actual, float expected, float tolerance)
{
  return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
  const OdometryConfig config = {
      .encoder_counts_per_revolution = 1320U,
      .wheel_diameter_m = 0.065f,
      .track_width_m = 0.220f,
  };
  const float circumference_m = PI_F * config.wheel_diameter_m;
  OdometrySnapshot snapshot;
  int32_t left_target;
  int32_t right_target;

  assert(DifferentialDrive_VelocityToWheelTargets(
      500, 0, 1320U, 0.065f, 0.220f, 10U, 100, &left_target,
      &right_target));
  assert(left_target == 32 && right_target == 32);
  assert(DifferentialDrive_VelocityToWheelTargets(
      0, 1000, 1320U, 0.065f, 0.220f, 10U, 100, &left_target,
      &right_target));
  assert(left_target == -7 && right_target == 7);
  assert(!DifferentialDrive_VelocityToWheelTargets(
      2000, 0, 1320U, 0.065f, 0.220f, 10U, 100, &left_target,
      &right_target));

  assert(!Odometry_Init(NULL));
  assert(!Odometry_Init(&(OdometryConfig){0}));
  assert(Odometry_Init(&config));
  Odometry_GetSnapshot(0U, &snapshot);
  assert(!snapshot.valid);

  assert(Odometry_Update(1320, 1320, 1000U, 1000U));
  Odometry_GetSnapshot(1100U, &snapshot);
  assert(snapshot.valid);
  assert(snapshot.sample_timestamp_ms == 1000U);
  assert(snapshot.sample_period_ms == 1000U);
  assert(snapshot.sample_age_ms == 100U);
  assert(snapshot.left_total == 1320);
  assert(snapshot.right_total == 1320);
  assert(NearlyEqual(snapshot.left_distance_m, circumference_m, 0.000001f));
  assert(NearlyEqual(snapshot.right_distance_m, circumference_m, 0.000001f));
  assert(NearlyEqual(snapshot.x_m, circumference_m, 0.000001f));
  assert(NearlyEqual(snapshot.y_m, 0.0f, 0.000001f));
  assert(NearlyEqual(snapshot.heading_rad, 0.0f, 0.000001f));
  assert(NearlyEqual(snapshot.linear_velocity_mps, circumference_m,
                     0.000001f));
  assert(NearlyEqual(snapshot.angular_velocity_rad_s, 0.0f, 0.000001f));

  Odometry_Reset();
  assert(Odometry_Update(-660, 660, 2000U, 1000U));
  Odometry_GetSnapshot(2000U, &snapshot);
  assert(NearlyEqual(snapshot.x_m, 0.0f, 0.000001f));
  assert(NearlyEqual(snapshot.y_m, 0.0f, 0.000001f));
  assert(NearlyEqual(snapshot.heading_rad,
                     circumference_m / config.track_width_m, 0.000001f));
  assert(NearlyEqual(snapshot.angular_velocity_rad_s,
                     circumference_m / config.track_width_m, 0.000001f));

  assert(Odometry_Update(-1320, 1320, 3000U, 1000U));
  assert(Odometry_Update(-1320, 1320, 4000U, 1000U));
  Odometry_GetSnapshot(4000U, &snapshot);
  assert(snapshot.heading_rad >= -PI_F);
  assert(snapshot.heading_rad < PI_F);
  assert(snapshot.heading_rad < 0.0f);

  Odometry_Reset();
  assert(Odometry_Update(0, 1320, UINT32_MAX - 50U, 1000U));
  Odometry_GetSnapshot(49U, &snapshot);
  assert(snapshot.sample_age_ms == 100U);
  assert(snapshot.x_m > 0.0f);
  assert(snapshot.y_m > 0.0f);
  assert(snapshot.heading_rad > 0.0f);

  Odometry_Reset();
  assert(!Odometry_Update(1, 1, 100U, 0U));
  Odometry_GetSnapshot(100U, &snapshot);
  assert(!snapshot.valid);
  return 0;
}

#endif
