#include "speed_pid.h"

#include <stddef.h>

void SpeedPid_Init(SpeedPid *pid, float kp, float ki, float kd,
                   float output_limit, float integral_limit)
{
  if (pid == NULL) {
    return;
  }

  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
  pid->output_limit = output_limit;
  pid->integral_limit = integral_limit;
  SpeedPid_Reset(pid);
}

void SpeedPid_Reset(SpeedPid *pid)
{
  if (pid == NULL) {
    return;
  }

  pid->integral = 0.0f;
  pid->previous_measurement = 0.0f;
  pid->initialized = false;
}

float SpeedPid_Update(SpeedPid *pid, float target, float measurement,
                      float dt_seconds)
{
  float error;
  float derivative = 0.0f;
  float next_integral;
  float output;

  if (pid == NULL || dt_seconds <= 0.0f || pid->output_limit <= 0.0f ||
      pid->integral_limit < 0.0f) {
    return 0.0f;
  }

  error = target - measurement;
  if (pid->initialized) {
    derivative = -(measurement - pid->previous_measurement) / dt_seconds;
  }

  next_integral = pid->integral + pid->ki * error * dt_seconds;
  if (next_integral > pid->integral_limit) {
    next_integral = pid->integral_limit;
  } else if (next_integral < -pid->integral_limit) {
    next_integral = -pid->integral_limit;
  }

  output = pid->kp * error + next_integral + pid->kd * derivative;
  if ((output > pid->output_limit && error > 0.0f) ||
      (output < -pid->output_limit && error < 0.0f)) {
    next_integral = pid->integral;
    output = pid->kp * error + next_integral + pid->kd * derivative;
  }

  if (output > pid->output_limit) {
    output = pid->output_limit;
  } else if (output < -pid->output_limit) {
    output = -pid->output_limit;
  }

  pid->integral = next_integral;
  pid->previous_measurement = measurement;
  pid->initialized = true;
  return output;
}
