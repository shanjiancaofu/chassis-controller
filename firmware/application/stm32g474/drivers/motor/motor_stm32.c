#include "drivers/motor/motor.h"

static const MotorStm32Config *config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static MotorStm32Data *data(const struct device *device)
{
  return device != NULL ? device->data : NULL;
}

static void Coast(const struct device *device)
{
  const MotorStm32Config *cfg = config(device);
  MotorStm32Data *state = data(device);
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (cfg != NULL && cfg->timer != NULL) {
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->left_positive_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->left_negative_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->right_positive_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->right_negative_channel, 0U);
  }
  if (state != NULL) {
    state->left_applied_duty = 0;
    state->right_applied_duty = 0;
  }
  __set_PRIMASK(primask);
}

static int Start(const struct device *device)
{
  const MotorStm32Config *cfg = config(device);
  if (cfg == NULL || cfg->timer == NULL || cfg->timer->Instance != TIM8) {
    return -1;
  }
  Coast(device);
  if (HAL_TIM_PWM_Start(cfg->timer, cfg->left_positive_channel) != HAL_OK ||
      HAL_TIM_PWM_Start(cfg->timer, cfg->left_negative_channel) != HAL_OK ||
      HAL_TIM_PWM_Start(cfg->timer, cfg->right_positive_channel) != HAL_OK ||
      HAL_TIM_PWM_Start(cfg->timer, cfg->right_negative_channel) != HAL_OK) {
    Coast(device);
    return -1;
  }
  Coast(device);
  return 0;
}

static void SetDuty(const struct device *device, MotorId motor, int16_t duty)
{
  const MotorStm32Config *cfg = config(device);
  MotorStm32Data *state = data(device);
  int32_t value = duty;
  const uint32_t primask = __get_PRIMASK();

  if (cfg == NULL || state == NULL || cfg->timer == NULL ||
      cfg->timer->Instance != TIM8) {
    return;
  }
  if (value > MOTOR_COMPARE_MAX) value = MOTOR_COMPARE_MAX;
  if (value < -MOTOR_COMPARE_MAX) value = -MOTOR_COMPARE_MAX;
  __disable_irq();
  if (state->emergency_stopped) {
    __set_PRIMASK(primask);
    return;
  }
  if (motor == MOTOR_LEFT) {
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->left_positive_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->left_negative_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer,
                          value >= 0 ? cfg->left_positive_channel
                                     : cfg->left_negative_channel,
                          (uint32_t)(value < 0 ? -value : value));
    state->left_applied_duty = (int16_t)value;
  } else if (motor == MOTOR_RIGHT) {
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->right_positive_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer, cfg->right_negative_channel, 0U);
    __HAL_TIM_SET_COMPARE(cfg->timer,
                          value >= 0 ? cfg->right_positive_channel
                                     : cfg->right_negative_channel,
                          (uint32_t)(value < 0 ? -value : value));
    state->right_applied_duty = (int16_t)value;
  }
  __set_PRIMASK(primask);
}

static void SetBoth(const struct device *device, int16_t left, int16_t right)
{
  SetDuty(device, MOTOR_LEFT, left);
  SetDuty(device, MOTOR_RIGHT, right);
}

static void EmergencyStop(const struct device *device)
{
  MotorStm32Data *state = data(device);
  if (state != NULL) state->emergency_stopped = true;
  Coast(device);
}

static void ClearEmergencyStop(const struct device *device)
{
  MotorStm32Data *state = data(device);
  if (state != NULL) state->emergency_stopped = false;
}

static int16_t GetDuty(const struct device *device, MotorId motor)
{
  const MotorStm32Data *state = data(device);
  if (state == NULL) return 0;
  return motor == MOTOR_LEFT ? state->left_applied_duty
                             : motor == MOTOR_RIGHT ? state->right_applied_duty : 0;
}

const MotorDriverApi motor_stm32_api = {
    .start = Start,
    .set_signed_duty = SetDuty,
    .set_signed_duty_both = SetBoth,
    .coast_all = Coast,
    .emergency_stop = EmergencyStop,
    .clear_emergency_stop = ClearEmergencyStop,
    .get_applied_duty = GetDuty,
};

int MotorStm32_Init(const struct device *device)
{
  MotorStm32Data *state = data(device);
  if (state == NULL) return -1;
  state->emergency_stopped = false;
  Coast(device);
  return 0;
}
