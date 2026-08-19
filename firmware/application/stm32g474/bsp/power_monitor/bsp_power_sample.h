#ifndef BSP_POWER_SAMPLE_H
#define BSP_POWER_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool valid;
  uint32_t millivolts;
  uint32_t sample_timestamp_ms;
  uint32_t sample_age_ms;
} BspPowerSampleSnapshot;

bool BspPowerSample_Init(void);
bool BspPowerSample_ReadRaw(uint16_t *raw);
bool BspPowerSample_ReadMillivolts(uint32_t *vin_mv);
bool BspPowerSample_GetLatestMillivolts(uint32_t *vin_mv);
void BspPowerSample_GetSnapshot(uint32_t now_ms,
                                BspPowerSampleSnapshot *snapshot);

#endif
