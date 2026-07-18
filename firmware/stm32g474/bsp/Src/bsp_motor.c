#include "bsp_motor.h"

#include "tim.h"

bool BspMotor_Init(void)
{
  BspMotor_Coast();

  if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK ||
      HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4) != HAL_OK) {
    BspMotor_Coast();
    return false;
  }

  return true;
}

void BspMotor_Set(int16_t left_per_mille, int16_t right_per_mille)
{
  int32_t left = left_per_mille;
  int32_t right = right_per_mille;
  const uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim8) + 1U;

  if (left > 1000) {
    left = 1000;
  } else if (left < -1000) {
    left = -1000;
  }
  if (right > 1000) {
    right = 1000;
  } else if (right < -1000) {
    right = -1000;
  }

  const uint32_t left_compare =
      (uint32_t)(left < 0 ? -left : left) * period / 1000U;
  const uint32_t right_compare =
      (uint32_t)(right < 0 ? -right : right) * period / 1000U;

  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1,
                        left >= 0 ? left_compare : 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2,
                        left < 0 ? left_compare : 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3,
                        right >= 0 ? right_compare : 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4,
                        right < 0 ? right_compare : 0U);
}

void BspMotor_Coast(void)
{
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0U);
}

void BspMotor_ReadEncoderDelta(int16_t *left_count, int16_t *right_count)
{
  static uint16_t previous_left;
  static uint16_t previous_right;
  const uint16_t current_left = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
  const uint16_t current_right = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);

  *left_count = (int16_t)(current_left - previous_left);
  *right_count = (int16_t)(current_right - previous_right);
  previous_left = current_left;
  previous_right = current_right;
}
