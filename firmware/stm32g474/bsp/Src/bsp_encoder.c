#include "bsp_encoder.h"

#include "project_config.h"
#include "tim.h"

static uint16_t previous_left_count;
static uint16_t previous_right_count;

void BspEncoder_Init(void)
{
  __HAL_TIM_SET_COUNTER(&htim2, 0U);
  __HAL_TIM_SET_COUNTER(&htim4, 0U);
  previous_left_count = 0U;
  previous_right_count = 0U;
}

bool BspEncoder_Start(void)
{
  BspEncoder_Init();
  if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK) {
    return false;
  }
  if (HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL) != HAL_OK) {
    HAL_TIM_Encoder_Stop(&htim2, TIM_CHANNEL_ALL);
    return false;
  }
  return true;
}

void BspEncoder_ReadDelta(int32_t *left_delta, int32_t *right_delta)
{
  const uint16_t current_left = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
  const uint16_t current_right = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);

  *left_delta =
      (int32_t)(int16_t)(current_left - previous_left_count) *
      MOTOR_LEFT_ENCODER_DIRECTION;
  *right_delta =
      (int32_t)(int16_t)(current_right - previous_right_count) *
      MOTOR_RIGHT_ENCODER_DIRECTION;
  previous_left_count = current_left;
  previous_right_count = current_right;
}
