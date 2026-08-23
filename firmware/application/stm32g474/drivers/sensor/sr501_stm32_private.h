#ifndef SR501_STM32_PRIVATE_H
#define SR501_STM32_PRIVATE_H

#include "drivers/gpio.h"
#include "drivers/sensor/sr501.h"

typedef struct {
  GpioSpec input;
} Sr501Stm32Config;

typedef struct {
  Sr501Status status;
  bool raw_high;
  bool stable_high;
  bool candidate_high;
  bool candidate_countable;
  uint32_t initialized_ms;
  uint32_t candidate_started_ms;
  uint32_t current_ms;
  uint32_t event_count;
  uint32_t last_motion_ms;
} Sr501Stm32Data;

extern const Sr501DriverApi sr501_stm32_api;
int Sr501Stm32_Init(const struct device *device);

#endif
