#ifndef EMERGENCY_STOP_H
#define EMERGENCY_STOP_H

#include <stdbool.h>
#include "device.h"

typedef void (*EmergencyStopLatchedCallback)(void);

typedef struct { bool(*asserted)(const struct device*); void(*set_callback)(const struct device*,EmergencyStopLatchedCallback); void(*on_interrupt)(const struct device*); } EmergencyStopDriverApi;
bool emergency_stop_is_asserted(const struct device*);
void emergency_stop_set_callback(const struct device*,EmergencyStopLatchedCallback);
void emergency_stop_on_interrupt(const struct device*);

#endif
