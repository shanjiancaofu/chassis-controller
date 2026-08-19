#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  TELEMETRY_MODE_OFF = 0,
  TELEMETRY_MODE_TEXT,
  TELEMETRY_MODE_VOFA
} TelemetryMode;

typedef struct {
  int32_t left_target;
  int32_t left_delta;
  int64_t left_total;
  int16_t left_output;
  int32_t right_target;
  int32_t right_delta;
  int64_t right_total;
  int16_t right_output;
  bool odometry_valid;
  uint32_t odometry_sample_timestamp_ms;
  uint32_t odometry_sample_age_ms;
  float odometry_x_m;
  float odometry_y_m;
  float odometry_heading_rad;
  float odometry_linear_velocity_mps;
  float odometry_angular_velocity_rad_s;
  int32_t supply_mv;
  uint32_t control_state;
  uint32_t fault_flags;
} TelemetrySnapshot;

void Telemetry_Init(void);
void Telemetry_SetMode(TelemetryMode mode);
TelemetryMode Telemetry_GetMode(void);
bool Telemetry_IsDue(uint32_t now_ms);
void Telemetry_Run(uint32_t now_ms, const TelemetrySnapshot *snapshot);

#endif
