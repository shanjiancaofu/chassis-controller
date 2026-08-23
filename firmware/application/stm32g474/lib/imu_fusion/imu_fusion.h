#ifndef IMU_FUSION_H
#define IMU_FUSION_H

#include <stdbool.h>
#include <stdint.h>

#define IMU_FUSION_CALIBRATION_SAMPLES 200U

typedef struct {
  float proportional_gain;
  float integral_gain;
  float gravity_mps2;
  float stationary_accel_tolerance_mps2;
  float stationary_gyro_limit_rad_s;
  float calibration_variance_limit;
  bool kalman_enabled;
  float kalman_angle_process_noise;
  float kalman_bias_process_noise;
  float kalman_measurement_noise;
} ImuFusionConfig;

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
} ImuFusionOutput;

typedef struct {
  ImuFusionConfig config;
  ImuFusionOutput output;
  float integral_error[3];
  float calibration_mean[3];
  float calibration_m2[3];
  float kalman_angle[2];
  float kalman_bias[2];
  float kalman_covariance[2][2][2];
} ImuFusion;

void ImuFusion_Init(ImuFusion *fusion, const ImuFusionConfig *config);
void ImuFusion_ResetCalibration(ImuFusion *fusion);
void ImuFusion_Update(ImuFusion *fusion, const float accel_mps2[3],
                      const float gyro_rad_s[3], float dt_seconds);
void ImuFusion_GetOutput(const ImuFusion *fusion, ImuFusionOutput *output);

#endif
