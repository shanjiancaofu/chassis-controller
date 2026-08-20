#ifndef POWER_SAMPLE_H
#define POWER_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

#include "device.h"

typedef struct {
  bool valid;
  uint32_t millivolts;
  uint32_t sample_timestamp_ms;
  uint32_t sample_age_ms;
} PowerSampleSnapshot;

typedef struct {
  int (*read_raw)(const struct device *device, uint16_t *raw);
  int (*read_millivolts)(const struct device *device, uint32_t *vin_mv);
  int (*get_latest_millivolts)(const struct device *device, uint32_t *vin_mv);
  void (*get_snapshot)(const struct device *device, uint32_t now_ms,
                       PowerSampleSnapshot *snapshot);
} PowerSampleDriverApi;

int power_sample_read_raw(const struct device *device, uint16_t *raw);
int power_sample_read_millivolts(const struct device *device, uint32_t *vin_mv);
int power_sample_get_latest_millivolts(const struct device *device,
                                       uint32_t *vin_mv);
void power_sample_get_snapshot(const struct device *device, uint32_t now_ms,
                               PowerSampleSnapshot *snapshot);
extern const PowerSampleDriverApi power_sample_stm32_api;
int PowerSampleStm32_Init(const struct device *device);

#endif
