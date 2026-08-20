#include "drivers/motor/motor.h"

#include "boards/chassis_g474/board_config.h"

static volatile bool emergency_stopped;
static int16_t left_applied_duty;
static int16_t right_applied_duty;

void Motor_Init(void)
{
  Motor_CoastAll();
}

bool Motor_Start(void)
{
  if (BOARD_MOTOR_TIMER.Instance != TIM8) {
    return false;
  }

  Motor_CoastAll();
  if (HAL_TIM_PWM_Start(&BOARD_MOTOR_TIMER,
                        BOARD_MOTOR_LEFT_POSITIVE_CHANNEL) != HAL_OK ||
      HAL_TIM_PWM_Start(&BOARD_MOTOR_TIMER,
                        BOARD_MOTOR_LEFT_NEGATIVE_CHANNEL) != HAL_OK ||
      HAL_TIM_PWM_Start(&BOARD_MOTOR_TIMER,
                        BOARD_MOTOR_RIGHT_NEGATIVE_CHANNEL) != HAL_OK ||
      HAL_TIM_PWM_Start(&BOARD_MOTOR_TIMER,
                        BOARD_MOTOR_RIGHT_POSITIVE_CHANNEL) != HAL_OK) {
    HAL_TIM_PWM_Stop(&BOARD_MOTOR_TIMER,
                     BOARD_MOTOR_LEFT_POSITIVE_CHANNEL);
    HAL_TIM_PWM_Stop(&BOARD_MOTOR_TIMER,
                     BOARD_MOTOR_LEFT_NEGATIVE_CHANNEL);
    HAL_TIM_PWM_Stop(&BOARD_MOTOR_TIMER,
                     BOARD_MOTOR_RIGHT_NEGATIVE_CHANNEL);
    HAL_TIM_PWM_Stop(&BOARD_MOTOR_TIMER,
                     BOARD_MOTOR_RIGHT_POSITIVE_CHANNEL);
    Motor_CoastAll();
    return false;
  }

  Motor_CoastAll();
  return true;
}

void Motor_SetSignedDuty(MotorId motor, int16_t duty)
{
  int32_t value = duty;
  uint32_t compare;
  const uint32_t primask = __get_PRIMASK();

  if (value > MOTOR_COMPARE_MAX) {
    value = MOTOR_COMPARE_MAX;
  } else if (value < -MOTOR_COMPARE_MAX) {
    value = -MOTOR_COMPARE_MAX;
  }
  compare = (uint32_t)(value < 0 ? -value : value);

  __disable_irq();
  if (emergency_stopped || BOARD_MOTOR_TIMER.Instance != TIM8) {
    __set_PRIMASK(primask);
    return;
  }

  if (motor == MOTOR_LEFT) {
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_LEFT_POSITIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_LEFT_NEGATIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(
        &BOARD_MOTOR_TIMER,
        value >= 0 ? BOARD_MOTOR_LEFT_POSITIVE_CHANNEL
                   : BOARD_MOTOR_LEFT_NEGATIVE_CHANNEL,
        compare);
    left_applied_duty = (int16_t)value;
  } else if (motor == MOTOR_RIGHT) {
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_RIGHT_NEGATIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_RIGHT_POSITIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(
        &BOARD_MOTOR_TIMER,
        value >= 0 ? BOARD_MOTOR_RIGHT_POSITIVE_CHANNEL
                   : BOARD_MOTOR_RIGHT_NEGATIVE_CHANNEL,
        compare);
    right_applied_duty = (int16_t)value;
  }
  __set_PRIMASK(primask);
}

void Motor_SetSignedDutyBoth(int16_t left_duty, int16_t right_duty)
{
  Motor_SetSignedDuty(MOTOR_LEFT, left_duty);
  Motor_SetSignedDuty(MOTOR_RIGHT, right_duty);
}

void Motor_CoastAll(void)
{
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (BOARD_MOTOR_TIMER.Instance == TIM8) {
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_LEFT_POSITIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_LEFT_NEGATIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_RIGHT_NEGATIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_RIGHT_POSITIVE_CHANNEL, 0U);
  }
  left_applied_duty = 0;
  right_applied_duty = 0;
  __set_PRIMASK(primask);
}

void Motor_EmergencyStop(void)
{
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  emergency_stopped = true;
  if (BOARD_MOTOR_TIMER.Instance == TIM8) {
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_LEFT_POSITIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_LEFT_NEGATIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_RIGHT_NEGATIVE_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&BOARD_MOTOR_TIMER,
                          BOARD_MOTOR_RIGHT_POSITIVE_CHANNEL, 0U);
  }
  left_applied_duty = 0;
  right_applied_duty = 0;
  __set_PRIMASK(primask);
}

void Motor_ClearEmergencyStop(void)
{
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  emergency_stopped = false;
  __set_PRIMASK(primask);
}

int16_t Motor_GetAppliedDuty(MotorId motor)
{
  if (motor == MOTOR_LEFT) {
    return left_applied_duty;
  }
  if (motor == MOTOR_RIGHT) {
    return right_applied_duty;
  }
  return 0;
}
