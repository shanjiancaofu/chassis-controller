#ifdef IMU_FUSION_HOST_TEST

#include <assert.h>
#include <math.h>

#include "components/imu_fusion/imu_fusion.h"

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
  assert(fabsf(output.quaternion[0] * output.quaternion[0] +
                   output.quaternion[1] * output.quaternion[1] +
                   output.quaternion[2] * output.quaternion[2] +
                   output.quaternion[3] * output.quaternion[3] -
               1.0f) <
         0.0001f);
  return 0;
}

#endif
