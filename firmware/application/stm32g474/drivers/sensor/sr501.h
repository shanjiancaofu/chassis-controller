#ifndef SR501_H
#define SR501_H

#include <stdbool.h>
#include <stdint.h>

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

#endif
