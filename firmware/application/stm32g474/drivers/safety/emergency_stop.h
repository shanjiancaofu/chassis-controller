#ifndef EMERGENCY_STOP_H
#define EMERGENCY_STOP_H

#include <stdbool.h>
#include "device.h"
#include "drivers/gpio.h"

typedef void (*EmergencyStopLatchedCallback)(void);

void EmergencyStop_Init(EmergencyStopLatchedCallback callback);
bool EmergencyStop_IsAsserted(void);
void EmergencyStop_OnInterrupt(void);
typedef struct { GpioSpec input; } EmergencyStopStm32Config;
typedef struct { bool(*asserted)(const struct device*); void(*set_callback)(const struct device*,EmergencyStopLatchedCallback); void(*on_interrupt)(const struct device*); } EmergencyStopDriverApi;
bool emergency_stop_is_asserted(const struct device*);
void emergency_stop_set_callback(const struct device*,EmergencyStopLatchedCallback);
void emergency_stop_on_interrupt(const struct device*);
extern const EmergencyStopDriverApi emergency_stop_stm32_api;
int EmergencyStopStm32_Init(const struct device*);

#endif
