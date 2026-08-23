#ifndef IMU_ORIENTATION_H
#define IMU_ORIENTATION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool calibrated;
  bool orientation_valid;
  uint16_t calibration_samples;
  float gyro_bias_rad_s[3];
  float quaternion[4];
  float roll_rad;
  float pitch_rad;
  float yaw_rad;
  bool kalman_valid;
  float kalman_roll_rad;
  float kalman_pitch_rad;
} ImuOrientationSnapshot;

void ImuOrientation_Init(void);
void ImuOrientation_Reset(void);
void ImuOrientation_ProcessSample(const float accel_mps2[3],
                                  const float gyro_rad_s[3],
                                  float sample_period_s);
void ImuOrientation_GetSnapshot(ImuOrientationSnapshot *snapshot);

#endif
