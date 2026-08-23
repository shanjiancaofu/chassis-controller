#ifdef IMU_FUSION_HOST_TEST

#include <assert.h>
#include <math.h>

#include "lib/imu_fusion/imu_fusion.h"

#define PI_F 3.14159265358979323846f

int main(void)
{
  const ImuFusionConfig config = {
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
  const float accel[3] = {0.0f, 0.0f, 9.80665f};
  const float gyro_bias[3] = {0.01f, -0.02f, 0.03f};
  ImuFusion fusion;
  ImuFusionOutput output;

  ImuFusion_Init(&fusion, &config);
  for (unsigned int sample = 0U;
       sample < IMU_FUSION_CALIBRATION_SAMPLES; ++sample) {
    ImuFusion_Update(&fusion, accel, gyro_bias, 0.01f);
  }
  ImuFusion_GetOutput(&fusion, &output);
  assert(output.calibrated);
  assert(output.orientation_valid);
  assert(output.kalman_valid);
  assert(fabsf(output.kalman_roll_rad) < 0.01f);
  assert(fabsf(output.kalman_pitch_rad) < 0.01f);
  assert(fabsf(output.gyro_bias_rad_s[0] - gyro_bias[0]) < 0.00001f);
  assert(fabsf(output.gyro_bias_rad_s[1] - gyro_bias[1]) < 0.00001f);
  assert(fabsf(output.gyro_bias_rad_s[2] - gyro_bias[2]) < 0.00001f);

  for (unsigned int sample = 0U; sample < 100U; ++sample) {
    const float gyro_turn[3] = {gyro_bias[0], gyro_bias[1],
                                gyro_bias[2] + PI_F * 0.5f};

    ImuFusion_Update(&fusion, accel, gyro_turn, 0.01f);
  }
  ImuFusion_GetOutput(&fusion, &output);
  assert(fabsf(output.roll_rad) < 0.01f);
  assert(fabsf(output.pitch_rad) < 0.01f);
  assert(fabsf(output.yaw_rad - PI_F * 0.5f) < 0.02f);
  assert(fabsf(output.kalman_roll_rad) < 0.02f);
  assert(fabsf(output.kalman_pitch_rad) < 0.02f);
  assert(fabsf(output.quaternion[0] * output.quaternion[0] +
                   output.quaternion[1] * output.quaternion[1] +
                   output.quaternion[2] * output.quaternion[2] +
                   output.quaternion[3] * output.quaternion[3] -
               1.0f) <
         0.0001f);

  {
    const float linear_accel[3] = {6.0f, 0.0f, 9.80665f};
    const float zero_gyro[3] = {0.0f, 0.0f, 0.0f};
    const float predicted_roll = output.kalman_roll_rad;

    ImuFusion_Update(&fusion, linear_accel, zero_gyro, 0.01f);
    ImuFusion_GetOutput(&fusion, &output);
    assert(output.kalman_valid);
    assert(fabsf(output.kalman_roll_rad - predicted_roll) < 0.05f);
  }

  {
    const float tilted_roll = 10.0f * PI_F / 180.0f;
    const float tilted_accel[3] = {
        0.0f, sinf(tilted_roll) * config.gravity_mps2,
        cosf(tilted_roll) * config.gravity_mps2};
    ImuFusion tilted_fusion;

    ImuFusion_Init(&tilted_fusion, &config);
    for (unsigned int sample = 0U;
         sample < IMU_FUSION_CALIBRATION_SAMPLES; ++sample) {
      ImuFusion_Update(&tilted_fusion, accel, gyro_bias, 0.01f);
    }
    for (unsigned int sample = 0U; sample < 200U; ++sample) {
      ImuFusion_Update(&tilted_fusion, tilted_accel, gyro_bias, 0.01f);
    }
    ImuFusion_GetOutput(&tilted_fusion, &output);
    assert(fabsf(output.kalman_roll_rad - tilted_roll) < 0.02f);
    assert(fabsf(tilted_fusion.kalman_bias[0] - gyro_bias[0]) < 0.005f);
    assert(fabsf(tilted_fusion.kalman_covariance[0][0][1] -
                 tilted_fusion.kalman_covariance[0][1][0]) < 0.0001f);
    assert(tilted_fusion.kalman_covariance[0][0][0] >= 0.0f);
    assert(tilted_fusion.kalman_covariance[0][1][1] >= 0.0f);
  }

  {
    const float near_positive_pi = 179.0f * PI_F / 180.0f;
    const float near_negative_pi = -179.0f * PI_F / 180.0f;
    const float initial_accel[3] = {
        0.0f, sinf(near_positive_pi) * config.gravity_mps2,
        cosf(near_positive_pi) * config.gravity_mps2};
    const float wrapped_accel[3] = {
        0.0f, sinf(near_negative_pi) * config.gravity_mps2,
        cosf(near_negative_pi) * config.gravity_mps2};
    const float zero_gyro[3] = {0.0f, 0.0f, 0.0f};
    ImuFusion wrapped_fusion;

    ImuFusion_Init(&wrapped_fusion, &config);
    for (unsigned int sample = 0U;
         sample < IMU_FUSION_CALIBRATION_SAMPLES; ++sample) {
      ImuFusion_Update(&wrapped_fusion, initial_accel, zero_gyro, 0.01f);
    }
    for (unsigned int sample = 0U; sample < 100U; ++sample) {
      ImuFusion_Update(&wrapped_fusion, wrapped_accel, zero_gyro, 0.01f);
    }
    ImuFusion_GetOutput(&wrapped_fusion, &output);
    assert(fabsf(fabsf(output.kalman_roll_rad) - PI_F) < 0.05f);
  }
  return 0;
}

#endif
