#ifndef CHASSIS_TIME_H
#define CHASSIS_TIME_H

#include <stdint.h>
#include "device.h"

uint32_t time_uptime_ms(void);
typedef struct { uint32_t (*uptime_ms)(const struct device *device); } TimeDriverApi;
extern const TimeDriverApi time_stm32_api;
int TimeStm32_Init(const struct device *device);

#endif
