#ifndef CHASSIS_GPIO_H
#define CHASSIS_GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "stm32g4xx_hal.h"

#define GPIO_ACTIVE_LOW (1U << 0)

typedef struct {
  const struct device *port;
  uint16_t pin;
  uint16_t flags;
} GpioSpec;

typedef struct {
  int (*get)(const struct device *device, uint16_t pin);
  int (*set)(const struct device *device, uint16_t pin, bool value);
  int (*toggle)(const struct device *device, uint16_t pin);
} GpioDriverApi;

typedef struct { GPIO_TypeDef *port; } GpioStm32Config;

int gpio_get(const GpioSpec *spec);
int gpio_set(const GpioSpec *spec, bool active);
int gpio_toggle(const GpioSpec *spec);
extern const GpioDriverApi gpio_stm32_api;
int GpioStm32_Init(const struct device *device);

#endif
