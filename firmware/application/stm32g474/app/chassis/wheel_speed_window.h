#ifndef WHEEL_SPEED_WINDOW_H
#define WHEEL_SPEED_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

#include "config/control_config.h"

#define WHEEL_SPEED_WINDOW_SAMPLES                                             \
  (MOTOR_CONTROL_REFERENCE_PERIOD_MS / MOTOR_CONTROL_PERIOD_MS)

typedef struct {
  int32_t left_samples[WHEEL_SPEED_WINDOW_SAMPLES];
  int32_t right_samples[WHEEL_SPEED_WINDOW_SAMPLES];
  int64_t left_sum;
  int64_t right_sum;
  uint32_t next_sample;
} WheelSpeedWindow;

void WheelSpeedWindow_Reset(WheelSpeedWindow *window);
bool WheelSpeedWindow_Update(WheelSpeedWindow *window, int32_t left_delta,
                             int32_t right_delta, uint32_t elapsed_ticks,
                             int32_t *left_measurement,
                             int32_t *right_measurement);

#endif
