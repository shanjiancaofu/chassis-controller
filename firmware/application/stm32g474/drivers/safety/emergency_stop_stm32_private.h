#ifndef CHASSIS_EMERGENCY_STOP_STM32_PRIVATE_H
#define CHASSIS_EMERGENCY_STOP_STM32_PRIVATE_H

#include "drivers/gpio.h"
#include "drivers/safety/emergency_stop.h"

typedef struct {
  GpioSpec input;
} EmergencyStopStm32Config;

typedef struct {
  EmergencyStopLatchedCallback latched_callback;
} EmergencyStopStm32Data;

extern const EmergencyStopDriverApi emergency_stop_stm32_api;
int EmergencyStopStm32_Init(const struct device *device);

#endif
