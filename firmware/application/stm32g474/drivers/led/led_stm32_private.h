#ifndef CHASSIS_LED_STM32_PRIVATE_H
#define CHASSIS_LED_STM32_PRIVATE_H

#include "drivers/gpio.h"
#include "drivers/led/led.h"

typedef struct {
  GpioSpec leds[3];
} LedStm32Config;

extern const LedDriverApi led_stm32_api;
int LedStm32_Init(const struct device *device);

#endif
