#ifndef CHASSIS_ENCODER_H
#define CHASSIS_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "device.h"
#include "stm32g4xx_hal.h"

typedef struct {
  TIM_HandleTypeDef *timer;
  int8_t direction;
} EncoderStm32Config;

typedef struct {
  uint16_t previous_count;
} EncoderStm32Data;

typedef struct {
  int (*start)(const struct device *device);
  int (*read_delta)(const struct device *device, int32_t *delta);
} EncoderDriverApi;

int encoder_start(const struct device *device);
int encoder_read_delta(const struct device *device, int32_t *delta);
extern const EncoderDriverApi encoder_stm32_api;
int EncoderStm32_Init(const struct device *device);

#endif
