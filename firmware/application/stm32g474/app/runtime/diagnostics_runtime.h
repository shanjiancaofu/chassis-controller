#ifndef DIAGNOSTICS_RUNTIME_H
#define DIAGNOSTICS_RUNTIME_H

#include <stdbool.h>

#include "device.h"

bool DiagnosticsRuntime_Init(const struct device *imu_device);
void DiagnosticsRuntime_Run(void);

#endif
