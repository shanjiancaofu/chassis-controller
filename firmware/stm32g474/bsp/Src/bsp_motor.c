#include "bsp_motor.h"

#include "tim.h"

static volatile bool emergency_stopped;
static int16_t left_applied_duty;
static int16_t right_applied_duty;

void BspMotor_Init(void)
{
  BspMotor_CoastAll();
}

bool BspMotor_Start(void)
{
  if (htim8.Instance != TIM8) {
    return false;
  }

  BspMotor_CoastAll();
  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4) != HAL_OK) {
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_3);
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
    BspMotor_CoastAll();
    return false;
  }

  BspMotor_CoastAll();
  return true;
}

void BspMotor_SetSignedDuty(BspMotorId motor, int16_t duty)
{
  int32_t value = duty;
  uint32_t compare;
  const uint32_t primask = __get_PRIMASK();

  if (value > BSP_MOTOR_COMPARE_MAX) {
    value = BSP_MOTOR_COMPARE_MAX;
  } else if (value < -BSP_MOTOR_COMPARE_MAX) {
    value = -BSP_MOTOR_COMPARE_MAX;
  }
  compare = (uint32_t)(value < 0 ? -value : value);

  __disable_irq();
  if (emergency_stopped || htim8.Instance != TIM8) {
    __set_PRIMASK(primask);
    return;
  }

  if (motor == BSP_MOTOR_LEFT) {
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, value >= 0 ? TIM_CHANNEL_1 : TIM_CHANNEL_2,
                          compare);
    left_applied_duty = (int16_t)value;
  } else if (motor == BSP_MOTOR_RIGHT) {
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, value >= 0 ? TIM_CHANNEL_4 : TIM_CHANNEL_3,
                          compare);
    right_applied_duty = (int16_t)value;
  }
  __set_PRIMASK(primask);
}

void BspMotor_SetSignedDutyBoth(int16_t left_duty, int16_t right_duty)
{
  BspMotor_SetSignedDuty(BSP_MOTOR_LEFT, left_duty);
  BspMotor_SetSignedDuty(BSP_MOTOR_RIGHT, right_duty);
}

void BspMotor_CoastAll(void)
{
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (htim8.Instance == TIM8) {
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0U);
  }
  left_applied_duty = 0;
  right_applied_duty = 0;
  __set_PRIMASK(primask);
}

void BspMotor_EmergencyStop(void)
{
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  emergency_stopped = true;
  if (htim8.Instance == TIM8) {
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0U);
  }
  left_applied_duty = 0;
  right_applied_duty = 0;
  __set_PRIMASK(primask);
}

void BspMotor_ClearEmergencyStop(void)
{
  const uint32_t primask = __get_PRIMASK();

  __disable_irq();
  emergency_stopped = false;
  __set_PRIMASK(primask);
}

int16_t BspMotor_GetAppliedDuty(BspMotorId motor)
{
  if (motor == BSP_MOTOR_LEFT) {
    return left_applied_duty;
  }
  if (motor == BSP_MOTOR_RIGHT) {
    return right_applied_duty;
  }
  return 0;
}
