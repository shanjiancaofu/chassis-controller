#include "app/modules/parameters/parameter_manager.h"

#include <stddef.h>

#include "config/control_config.h"

static ParameterSnapshot active_parameters;
static ParameterSnapshot pending_parameters;
static uint8_t pending_wheels;

enum {
  PENDING_LEFT = 1U << 0,
  PENDING_RIGHT = 1U << 1
};

static ParameterSnapshot DefaultParameters(void);

void ParameterManager_Init(const ParameterSnapshot *initial_parameters)
{
  active_parameters = DefaultParameters();
  if (initial_parameters != NULL &&
      initial_parameters->left_pid.kp <= MOTOR_PID_KP_MAX &&
      initial_parameters->left_pid.ki <= MOTOR_PID_KI_MAX &&
      initial_parameters->left_pid.kd <= MOTOR_PID_KD_MAX &&
      initial_parameters->right_pid.kp <= MOTOR_PID_KP_MAX &&
      initial_parameters->right_pid.ki <= MOTOR_PID_KI_MAX &&
      initial_parameters->right_pid.kd <= MOTOR_PID_KD_MAX) {
    active_parameters = *initial_parameters;
  }
  pending_parameters = active_parameters;
  pending_wheels = 0U;
}

bool ParameterManager_StagePidGains(ParameterWheel wheel, uint16_t kp,
                                    uint16_t ki, uint16_t kd)
{
  ParameterPidGains *gains;
  uint8_t pending_bit;

  if ((wheel != PARAMETER_WHEEL_LEFT &&
       wheel != PARAMETER_WHEEL_RIGHT) ||
      kp > MOTOR_PID_KP_MAX || ki > MOTOR_PID_KI_MAX ||
      kd > MOTOR_PID_KD_MAX) {
    return false;
  }

  gains = wheel == PARAMETER_WHEEL_LEFT
              ? &pending_parameters.left_pid
              : &pending_parameters.right_pid;
  pending_bit = wheel == PARAMETER_WHEEL_LEFT
                    ? PENDING_LEFT
                    : PENDING_RIGHT;
  *gains = (ParameterPidGains){.kp = kp, .ki = ki, .kd = kd};
  pending_wheels |= pending_bit;
  return true;
}

bool ParameterManager_ApplyPending(ParameterSnapshot *parameters)
{
  if (pending_wheels == 0U) {
    return false;
  }

  if ((pending_wheels & PENDING_LEFT) != 0U) {
    active_parameters.left_pid = pending_parameters.left_pid;
  }
  if ((pending_wheels & PENDING_RIGHT) != 0U) {
    active_parameters.right_pid = pending_parameters.right_pid;
  }
  pending_wheels = 0U;
  if (parameters != NULL) {
    *parameters = active_parameters;
  }
  return true;
}

void ParameterManager_GetRequested(ParameterSnapshot *parameters)
{
  if (parameters != NULL) {
    *parameters = active_parameters;
    if ((pending_wheels & PENDING_LEFT) != 0U) {
      parameters->left_pid = pending_parameters.left_pid;
    }
    if ((pending_wheels & PENDING_RIGHT) != 0U) {
      parameters->right_pid = pending_parameters.right_pid;
    }
  }
}

void ParameterManager_GetActive(ParameterSnapshot *parameters)
{
  if (parameters != NULL) {
    *parameters = active_parameters;
  }
}

void ParameterManager_RestoreDefaults(void)
{
  pending_parameters = DefaultParameters();
  pending_wheels = PENDING_LEFT | PENDING_RIGHT;
}

static ParameterSnapshot DefaultParameters(void)
{
  return (ParameterSnapshot){
      .left_pid = {
          .kp = (uint16_t)MOTOR_LEFT_PID_KP,
          .ki = (uint16_t)MOTOR_LEFT_PID_KI,
          .kd = (uint16_t)MOTOR_LEFT_PID_KD,
      },
      .right_pid = {
          .kp = (uint16_t)MOTOR_RIGHT_PID_KP,
          .ki = (uint16_t)MOTOR_RIGHT_PID_KI,
          .kd = (uint16_t)MOTOR_RIGHT_PID_KD,
      },
  };
}
