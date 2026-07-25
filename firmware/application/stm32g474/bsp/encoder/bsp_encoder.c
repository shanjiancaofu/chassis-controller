#include "bsp/encoder/bsp_encoder.h"

#include "board/board_config.h"

static uint16_t previous_left_count;
static uint16_t previous_right_count;

void BspEncoder_Init(void)
{
  __HAL_TIM_SET_COUNTER(&BOARD_LEFT_ENCODER_TIMER, 0U);
  __HAL_TIM_SET_COUNTER(&BOARD_RIGHT_ENCODER_TIMER, 0U);
  previous_left_count = 0U;
  previous_right_count = 0U;
}

bool BspEncoder_Start(void)
{
  BspEncoder_Init();
  if (HAL_TIM_Encoder_Start(&BOARD_LEFT_ENCODER_TIMER, TIM_CHANNEL_ALL) !=
      HAL_OK) {
    return false;
  }
  if (HAL_TIM_Encoder_Start(&BOARD_RIGHT_ENCODER_TIMER, TIM_CHANNEL_ALL) !=
      HAL_OK) {
    HAL_TIM_Encoder_Stop(&BOARD_LEFT_ENCODER_TIMER, TIM_CHANNEL_ALL);
    return false;
  }
  return true;
}

void BspEncoder_ReadDelta(int32_t *left_delta, int32_t *right_delta)
{
  const uint16_t current_left =
      (uint16_t)__HAL_TIM_GET_COUNTER(&BOARD_LEFT_ENCODER_TIMER);
  const uint16_t current_right =
      (uint16_t)__HAL_TIM_GET_COUNTER(&BOARD_RIGHT_ENCODER_TIMER);

  *left_delta =
      (int32_t)(int16_t)(current_left - previous_left_count) *
      BOARD_LEFT_ENCODER_DIRECTION;
  *right_delta =
      (int32_t)(int16_t)(current_right - previous_right_count) *
      BOARD_RIGHT_ENCODER_DIRECTION;
  previous_left_count = current_left;
  previous_right_count = current_right;
}
