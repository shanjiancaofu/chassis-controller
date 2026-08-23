#ifndef SR501_H
#define SR501_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "drivers/gpio.h"

typedef enum {
  SR501_WARMING_UP = 0,
  SR501_READY
} Sr501Status;

typedef struct {
  Sr501Status status;
  bool raw_high;
  bool motion_detected;
  uint32_t event_count;
  uint32_t last_motion_ms;
  uint32_t warmup_remaining_ms;
} Sr501Snapshot;

void Sr501_Init(uint32_t now_ms);
void Sr501_Run(uint32_t now_ms);
void Sr501_GetSnapshot(Sr501Snapshot *snapshot);
typedef struct { GpioSpec input; } Sr501Stm32Config;
typedef struct { void(*run)(const struct device*,uint32_t); void(*snapshot)(const struct device*,Sr501Snapshot*); } Sr501DriverApi;
void sr501_run(const struct device*,uint32_t);
void sr501_get_snapshot(const struct device*,Sr501Snapshot*);
extern const Sr501DriverApi sr501_stm32_api;
int Sr501Stm32_Init(const struct device*);

#endif
