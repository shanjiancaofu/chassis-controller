#include "lib/imu_fusion/imu_fusion.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define HALF 0.5f
#define PI 3.14159265358979323846f
#define TWO_PI 6.28318530717958647692f
#define MAX_UPDATE_PERIOD_SECONDS 0.1f
#define BIAS_TRACKING_TIME_CONSTANT_SECONDS 30.0f
#define DEFAULT_KALMAN_ANGLE_PROCESS_NOISE 0.001f
#define DEFAULT_KALMAN_BIAS_PROCESS_NOISE 0.003f
#define DEFAULT_KALMAN_MEASUREMENT_NOISE 0.03f

static bool IsStationary(const ImuFusion *fusion, const float accel[3],
                         const float gyro[3]);
static void AccumulateCalibration(ImuFusion *fusion, const float accel[3],
                                  const float gyro[3]);
static void InitializeTilt(ImuFusion *fusion, const float accel[3]);
static void UpdateQuaternion(ImuFusion *fusion, const float accel[3],
                             const float gyro[3], float dt_seconds);
static void UpdateEulerAngles(ImuFusion *fusion);
static void InitializeKalman(ImuFusion *fusion, const float accel[3]);
static void UpdateKalman(ImuFusion *fusion, const float accel[3],
                         const float gyro[3], float dt_seconds);
static void UpdateKalmanAxis(ImuFusion *fusion, uint32_t axis, float rate,
                             float measurement, float dt_seconds,
                             bool measurement_valid);
static float Clamp(float value, float minimum, float maximum);
static float WrapAngle(float angle);

void ImuFusion_Init(ImuFusion *fusion, const ImuFusionConfig *config)
{
  if (fusion == NULL || config == NULL) {
    return;
  }
  memset(fusion, 0, sizeof(*fusion));
  fusion->config = *config;
  if (fusion->config.kalman_angle_process_noise <= 0.0f) {
    fusion->config.kalman_angle_process_noise =
        DEFAULT_KALMAN_ANGLE_PROCESS_NOISE;
  }
  if (fusion->config.kalman_bias_process_noise <= 0.0f) {
    fusion->config.kalman_bias_process_noise =
        DEFAULT_KALMAN_BIAS_PROCESS_NOISE;
  }
  if (fusion->config.kalman_measurement_noise <= 0.0f) {
    fusion->config.kalman_measurement_noise =
        DEFAULT_KALMAN_MEASUREMENT_NOISE;
  }
  fusion->output.quaternion[0] = 1.0f;
}

void ImuFusion_ResetCalibration(ImuFusion *fusion)
{
  ImuFusionConfig config;

  if (fusion == NULL) {
    return;
  }
  config = fusion->config;
  ImuFusion_Init(fusion, &config);
}

void ImuFusion_Update(ImuFusion *fusion, const float accel_mps2[3],
                      const float gyro_rad_s[3], float dt_seconds)
{
  float corrected_gyro[3];

  if (fusion == NULL || accel_mps2 == NULL || gyro_rad_s == NULL ||
      dt_seconds <= 0.0f || dt_seconds > MAX_UPDATE_PERIOD_SECONDS) {
    return;
  }
  if (!fusion->output.calibrated) {
    AccumulateCalibration(fusion, accel_mps2, gyro_rad_s);
    return;
  }
  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    corrected_gyro[axis] =
        gyro_rad_s[axis] - fusion->output.gyro_bias_rad_s[axis];
  }
  if (IsStationary(fusion, accel_mps2, corrected_gyro)) {
    const float alpha =
        Clamp(dt_seconds / BIAS_TRACKING_TIME_CONSTANT_SECONDS, 0.0f, 1.0f);

    for (uint32_t axis = 0U; axis < 3U; ++axis) {
      fusion->output.gyro_bias_rad_s[axis] += corrected_gyro[axis] * alpha;
      corrected_gyro[axis] =
          gyro_rad_s[axis] - fusion->output.gyro_bias_rad_s[axis];
    }
  }
  UpdateQuaternion(fusion, accel_mps2, corrected_gyro, dt_seconds);
  UpdateEulerAngles(fusion);
  UpdateKalman(fusion, accel_mps2, gyro_rad_s, dt_seconds);
  fusion->output.orientation_valid = true;
}

void ImuFusion_GetOutput(const ImuFusion *fusion, ImuFusionOutput *output)
{
  if (fusion != NULL && output != NULL) {
    *output = fusion->output;
  }
}

static bool IsStationary(const ImuFusion *fusion, const float accel[3],
                         const float gyro[3])
{
  const float accel_norm =
      sqrtf(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
  const float gyro_norm =
      sqrtf(gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2]);

  return fabsf(accel_norm - fusion->config.gravity_mps2) <=
             fusion->config.stationary_accel_tolerance_mps2 &&
         gyro_norm <= fusion->config.stationary_gyro_limit_rad_s;
}

static void AccumulateCalibration(ImuFusion *fusion, const float accel[3],
                                  const float gyro[3])
{
  if (!IsStationary(fusion, accel, gyro)) {
    memset(fusion->calibration_mean, 0, sizeof(fusion->calibration_mean));
    memset(fusion->calibration_m2, 0, sizeof(fusion->calibration_m2));
    fusion->output.calibration_samples = 0U;
    return;
  }

  ++fusion->output.calibration_samples;
  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    const float delta = gyro[axis] - fusion->calibration_mean[axis];

    fusion->calibration_mean[axis] +=
        delta / (float)fusion->output.calibration_samples;
    fusion->calibration_m2[axis] +=
        delta * (gyro[axis] - fusion->calibration_mean[axis]);
  }
  if (fusion->output.calibration_samples < IMU_FUSION_CALIBRATION_SAMPLES) {
    return;
  }
  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    const float variance = fusion->calibration_m2[axis] /
                           (float)(IMU_FUSION_CALIBRATION_SAMPLES - 1U);

    if (variance > fusion->config.calibration_variance_limit) {
      fusion->output.calibration_samples = 0U;
      memset(fusion->calibration_mean, 0, sizeof(fusion->calibration_mean));
      memset(fusion->calibration_m2, 0, sizeof(fusion->calibration_m2));
      return;
    }
  }
  memcpy(fusion->output.gyro_bias_rad_s, fusion->calibration_mean,
         sizeof(fusion->output.gyro_bias_rad_s));
  memset(fusion->integral_error, 0, sizeof(fusion->integral_error));
  InitializeTilt(fusion, accel);
  InitializeKalman(fusion, accel);
  UpdateEulerAngles(fusion);
  fusion->output.calibrated = true;
  fusion->output.orientation_valid = true;
}

static void InitializeKalman(ImuFusion *fusion, const float accel[3])
{
  const float roll = atan2f(accel[1], accel[2]);
  const float pitch =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2]));

  fusion->kalman_angle[0] = roll;
  fusion->kalman_angle[1] = pitch;
  fusion->kalman_bias[0] = fusion->output.gyro_bias_rad_s[0];
  fusion->kalman_bias[1] = fusion->output.gyro_bias_rad_s[1];
  memset(fusion->kalman_covariance, 0, sizeof(fusion->kalman_covariance));
  fusion->kalman_covariance[0][0][0] = 0.1f;
  fusion->kalman_covariance[0][1][1] = 0.1f;
  fusion->kalman_covariance[1][0][0] = 0.1f;
  fusion->kalman_covariance[1][1][1] = 0.1f;
  fusion->output.kalman_roll_rad = roll;
  fusion->output.kalman_pitch_rad = pitch;
  fusion->output.kalman_valid = fusion->config.kalman_enabled;
}

static void UpdateKalman(ImuFusion *fusion, const float accel[3],
                         const float gyro[3], float dt_seconds)
{
  const float accel_norm =
      sqrtf(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
  const bool measurement_valid =
      accel_norm > 0.001f &&
      fabsf(accel_norm - fusion->config.gravity_mps2) <=
          fusion->config.stationary_accel_tolerance_mps2 * 2.0f;
  const float roll_measurement = atan2f(accel[1], accel[2]);
  const float pitch_measurement =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2]));

  if (!fusion->config.kalman_enabled || !fusion->output.kalman_valid) {
    return;
  }
  UpdateKalmanAxis(fusion, 0U, gyro[0], roll_measurement, dt_seconds,
                   measurement_valid);
  UpdateKalmanAxis(fusion, 1U, gyro[1], pitch_measurement, dt_seconds,
                   measurement_valid);
  fusion->output.kalman_roll_rad = fusion->kalman_angle[0];
  fusion->output.kalman_pitch_rad = fusion->kalman_angle[1];
}

static void UpdateKalmanAxis(ImuFusion *fusion, uint32_t axis, float rate,
                             float measurement, float dt_seconds,
                             bool measurement_valid)
{
  float (*covariance)[2] = fusion->kalman_covariance[axis];
  const float old_p00 = covariance[0][0];
  const float old_p01 = covariance[0][1];
  const float old_p10 = covariance[1][0];
  const float old_p11 = covariance[1][1];
  const float process_angle = fusion->config.kalman_angle_process_noise;
  const float process_bias = fusion->config.kalman_bias_process_noise;
  const float measurement_noise = fusion->config.kalman_measurement_noise;
  float innovation;
  float innovation_variance;
  float gain_angle;
  float gain_bias;
  float predicted_p00;
  float predicted_p01;

  fusion->kalman_angle[axis] +=
      dt_seconds * (rate - fusion->kalman_bias[axis]);
  covariance[0][0] = old_p00 +
                      dt_seconds *
                          (dt_seconds * old_p11 - old_p01 - old_p10 +
                           process_angle);
  covariance[0][1] = old_p01 - dt_seconds * old_p11;
  covariance[1][0] = old_p10 - dt_seconds * old_p11;
  covariance[1][1] = old_p11 + process_bias * dt_seconds;

  if (!measurement_valid || fabsf(measurement) > 4.0f) {
    return;
  }
  innovation = WrapAngle(measurement - fusion->kalman_angle[axis]);
  innovation_variance = covariance[0][0] + measurement_noise;
  if (innovation_variance <= 0.0f) {
    return;
  }
  gain_angle = covariance[0][0] / innovation_variance;
  gain_bias = covariance[1][0] / innovation_variance;
  fusion->kalman_angle[axis] += gain_angle * innovation;
  fusion->kalman_bias[axis] += gain_bias * innovation;
  predicted_p00 = covariance[0][0];
  predicted_p01 = covariance[0][1];
  covariance[0][0] -= gain_angle * predicted_p00;
  covariance[0][1] -= gain_angle * predicted_p01;
  covariance[1][0] -= gain_bias * predicted_p00;
  covariance[1][1] -= gain_bias * predicted_p01;
}

static float WrapAngle(float angle)
{
  while (angle > PI) {
    angle -= TWO_PI;
  }
  while (angle < -PI) {
    angle += TWO_PI;
  }
  return angle;
}

static void InitializeTilt(ImuFusion *fusion, const float accel[3])
{
  const float roll = atan2f(accel[1], accel[2]);
  const float pitch =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2]));
  const float half_roll = roll * HALF;
  const float half_pitch = pitch * HALF;
  const float cr = cosf(half_roll);
  const float sr = sinf(half_roll);
  const float cp = cosf(half_pitch);
  const float sp = sinf(half_pitch);

  fusion->output.quaternion[0] = cr * cp;
  fusion->output.quaternion[1] = sr * cp;
  fusion->output.quaternion[2] = cr * sp;
  fusion->output.quaternion[3] = -sr * sp;
}

static void UpdateQuaternion(ImuFusion *fusion, const float accel[3],
                             const float gyro[3], float dt_seconds)
{
  float ax = accel[0];
  float ay = accel[1];
  float az = accel[2];
  float gx = gyro[0];
  float gy = gyro[1];
  float gz = gyro[2];
  float q0 = fusion->output.quaternion[0];
  float q1 = fusion->output.quaternion[1];
  float q2 = fusion->output.quaternion[2];
  float q3 = fusion->output.quaternion[3];
  const float accel_norm = sqrtf(ax * ax + ay * ay + az * az);

  if (accel_norm > 0.001f &&
      fabsf(accel_norm - fusion->config.gravity_mps2) <=
          fusion->config.stationary_accel_tolerance_mps2 * 2.0f) {
    const float reciprocal_norm = 1.0f / accel_norm;
    float error[3];
    float gravity[3];

    ax *= reciprocal_norm;
    ay *= reciprocal_norm;
    az *= reciprocal_norm;
    gravity[0] = 2.0f * (q1 * q3 - q0 * q2);
    gravity[1] = 2.0f * (q0 * q1 + q2 * q3);
    gravity[2] = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;
    error[0] = ay * gravity[2] - az * gravity[1];
    error[1] = az * gravity[0] - ax * gravity[2];
    error[2] = ax * gravity[1] - ay * gravity[0];
    for (uint32_t axis = 0U; axis < 3U; ++axis) {
      fusion->integral_error[axis] +=
          fusion->config.integral_gain * error[axis] * dt_seconds;
    }
    gx += fusion->config.proportional_gain * error[0] +
          fusion->integral_error[0];
    gy += fusion->config.proportional_gain * error[1] +
          fusion->integral_error[1];
    gz += fusion->config.proportional_gain * error[2] +
          fusion->integral_error[2];
  }

  {
    const float half_dt = HALF * dt_seconds;
    const float next_q0 = q0 + (-q1 * gx - q2 * gy - q3 * gz) * half_dt;
    const float next_q1 = q1 + (q0 * gx + q2 * gz - q3 * gy) * half_dt;
    const float next_q2 = q2 + (q0 * gy - q1 * gz + q3 * gx) * half_dt;
    const float next_q3 = q3 + (q0 * gz + q1 * gy - q2 * gx) * half_dt;
    const float reciprocal_norm =
        1.0f / sqrtf(next_q0 * next_q0 + next_q1 * next_q1 +
                     next_q2 * next_q2 + next_q3 * next_q3);

    fusion->output.quaternion[0] = next_q0 * reciprocal_norm;
    fusion->output.quaternion[1] = next_q1 * reciprocal_norm;
    fusion->output.quaternion[2] = next_q2 * reciprocal_norm;
    fusion->output.quaternion[3] = next_q3 * reciprocal_norm;
  }
}

static void UpdateEulerAngles(ImuFusion *fusion)
{
  const float q0 = fusion->output.quaternion[0];
  const float q1 = fusion->output.quaternion[1];
  const float q2 = fusion->output.quaternion[2];
  const float q3 = fusion->output.quaternion[3];

  fusion->output.roll_rad =
      atan2f(2.0f * (q0 * q1 + q2 * q3),
             1.0f - 2.0f * (q1 * q1 + q2 * q2));
  fusion->output.pitch_rad =
      asinf(Clamp(2.0f * (q0 * q2 - q3 * q1), -1.0f, 1.0f));
  fusion->output.yaw_rad =
      atan2f(2.0f * (q0 * q3 + q1 * q2),
             1.0f - 2.0f * (q2 * q2 + q3 * q3));
}

static float Clamp(float value, float minimum, float maximum)
{
  if (value < minimum) {
    return minimum;
  }
  return value > maximum ? maximum : value;
}
