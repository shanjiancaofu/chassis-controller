#ifndef POWER_SAMPLE_STM32_PRIVATE_H
#define POWER_SAMPLE_STM32_PRIVATE_H

#include "drivers/adc/power_sample.h"

typedef struct {
  volatile uint32_t latest_millivolts;
  volatile uint32_t latest_sample_timestamp_ms;
  volatile bool latest_valid;
} PowerSampleStm32Data;

extern const PowerSampleDriverApi power_sample_stm32_api;
int PowerSampleStm32_Init(const struct device *device);

#endif
