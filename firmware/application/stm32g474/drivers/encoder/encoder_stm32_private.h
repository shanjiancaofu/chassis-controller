#ifndef CHASSIS_ENCODER_STM32_PRIVATE_H
#define CHASSIS_ENCODER_STM32_PRIVATE_H

#include "drivers/encoder/encoder.h"
#include "stm32g4xx_hal.h"

typedef struct {
  TIM_HandleTypeDef *timer;
  int8_t direction;
} EncoderStm32Config;

typedef struct {
  uint16_t previous_count;
} EncoderStm32Data;

extern const EncoderDriverApi encoder_stm32_api;
int EncoderStm32_Init(const struct device *device);

#endif
