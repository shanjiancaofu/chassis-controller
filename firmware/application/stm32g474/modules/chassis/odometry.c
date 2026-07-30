#include "modules/chassis/odometry.h"

#include <limits.h>
#include <stddef.h>

#include "modules/chassis/differential_drive.h"

static OdometrySnapshot odometry_snapshot;

static int64_t SaturatingAdd(int64_t value, int32_t increment);

void Odometry_Init(void)
{
  odometry_snapshot = (OdometrySnapshot){0};
}

void Odometry_Update(int32_t left_delta, int32_t right_delta)
{
  DifferentialDriveMotionDelta motion_delta;

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
}

void Odometry_Reset(void)
{
  odometry_snapshot = (OdometrySnapshot){0};
}

void Odometry_GetSnapshot(OdometrySnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = odometry_snapshot;
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
