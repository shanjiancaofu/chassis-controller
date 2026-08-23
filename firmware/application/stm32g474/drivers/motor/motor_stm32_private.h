#ifndef CHASSIS_MOTOR_STM32_PRIVATE_H
#define CHASSIS_MOTOR_STM32_PRIVATE_H

#include "drivers/motor/motor.h"
#include "stm32g4xx_hal.h"

typedef struct {
  TIM_HandleTypeDef *timer;
  uint32_t left_positive_channel;
  uint32_t left_negative_channel;
  uint32_t right_positive_channel;
  uint32_t right_negative_channel;
} MotorStm32Config;

typedef struct {
  volatile bool emergency_stopped;
  int16_t left_applied_duty;
  int16_t right_applied_duty;
} MotorStm32Data;

extern const MotorDriverApi motor_stm32_api;
int MotorStm32_Init(const struct device *device);

#endif
