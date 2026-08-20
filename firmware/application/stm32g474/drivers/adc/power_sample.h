#ifndef POWER_SAMPLE_H
#define POWER_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool valid;
  uint32_t millivolts;
  uint32_t sample_timestamp_ms;
  uint32_t sample_age_ms;
} PowerSampleSnapshot;

bool PowerSample_Init(void);
bool PowerSample_ReadRaw(uint16_t *raw);
bool PowerSample_ReadMillivolts(uint32_t *vin_mv);
bool PowerSample_GetLatestMillivolts(uint32_t *vin_mv);
void PowerSample_GetSnapshot(uint32_t now_ms,
                                PowerSampleSnapshot *snapshot);

#endif
