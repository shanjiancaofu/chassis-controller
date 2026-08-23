#ifndef SR501_H
#define SR501_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

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

typedef struct { void(*run)(const struct device*,uint32_t); void(*snapshot)(const struct device*,Sr501Snapshot*); } Sr501DriverApi;
void sr501_run(const struct device*,uint32_t);
void sr501_get_snapshot(const struct device*,Sr501Snapshot*);

#endif
