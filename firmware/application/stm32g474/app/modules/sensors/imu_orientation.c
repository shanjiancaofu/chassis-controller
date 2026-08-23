#include "app/modules/sensors/imu_orientation.h"

#include <stddef.h>

#include "lib/imu_fusion/imu_fusion.h"

static const ImuFusionConfig fusion_config = {
    .proportional_gain = 1.5f,
    .integral_gain = 0.05f,
    .gravity_mps2 = 9.80665f,
    .stationary_accel_tolerance_mps2 = 0.8f,
    .stationary_gyro_limit_rad_s = 0.2f,
    .calibration_variance_limit = 0.00001f,
    .kalman_enabled = true,
    .kalman_angle_process_noise = 0.001f,
    .kalman_bias_process_noise = 0.003f,
    .kalman_measurement_noise = 0.03f,
};

static ImuFusion fusion;

void ImuOrientation_Init(void)
{
  ImuFusion_Init(&fusion, &fusion_config);
}

void ImuOrientation_Reset(void)
{
  ImuFusion_ResetCalibration(&fusion);
}

void ImuOrientation_ProcessSample(const float accel_mps2[3],
                                  const float gyro_rad_s[3],
                                  float sample_period_s)
{
  ImuFusion_Update(&fusion, accel_mps2, gyro_rad_s, sample_period_s);
}

void ImuOrientation_GetSnapshot(ImuOrientationSnapshot *snapshot)
{
  ImuFusionOutput output;

  if (snapshot == NULL) {
    return;
  }
  ImuFusion_GetOutput(&fusion, &output);
  snapshot->calibrated = output.calibrated;
  snapshot->orientation_valid = output.orientation_valid;
  snapshot->calibration_samples = output.calibration_samples;
  for (uint32_t index = 0U; index < 3U; ++index) {
    snapshot->gyro_bias_rad_s[index] = output.gyro_bias_rad_s[index];
  }
  for (uint32_t index = 0U; index < 4U; ++index) {
    snapshot->quaternion[index] = output.quaternion[index];
  }
  snapshot->roll_rad = output.roll_rad;
  snapshot->pitch_rad = output.pitch_rad;
  snapshot->yaw_rad = output.yaw_rad;
  snapshot->kalman_valid = output.kalman_valid;
  snapshot->kalman_roll_rad = output.kalman_roll_rad;
  snapshot->kalman_pitch_rad = output.kalman_pitch_rad;
}
