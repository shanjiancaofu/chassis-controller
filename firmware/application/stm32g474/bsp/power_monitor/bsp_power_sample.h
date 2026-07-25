#ifndef BSP_POWER_SAMPLE_H
#define BSP_POWER_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

bool BspPowerSample_Init(void);
bool BspPowerSample_ReadRaw(uint16_t *raw);
bool BspPowerSample_ReadMillivolts(uint32_t *vin_mv);

#endif
