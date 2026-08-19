#ifndef ODOMETRY_H
#define ODOMETRY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t encoder_counts_per_revolution;
  float wheel_diameter_m;
  float track_width_m;
} OdometryConfig;

typedef struct {
  bool valid;
  uint32_t sample_timestamp_ms;
  uint32_t sample_period_ms;
  uint32_t sample_age_ms;
  int32_t left_delta;
  int32_t right_delta;
  int64_t left_total;
  int64_t right_total;
  int32_t forward_count_sum_delta;
  int32_t turn_count_difference_delta;
  int64_t forward_count_sum_total;
  int64_t turn_count_difference_total;
  float left_distance_m;
  float right_distance_m;
  float distance_delta_m;
  float heading_delta_rad;
  float x_m;
  float y_m;
  float heading_rad;
  float linear_velocity_mps;
  float angular_velocity_rad_s;
} OdometrySnapshot;

bool Odometry_Init(const OdometryConfig *config);
bool Odometry_Update(int32_t left_delta, int32_t right_delta,
                     uint32_t sample_timestamp_ms,
                     uint32_t sample_period_ms);
void Odometry_Reset(void);
void Odometry_GetSnapshot(uint32_t now_ms, OdometrySnapshot *snapshot);

#endif
