#ifndef BSP_SR501_H
#define BSP_SR501_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BSP_SR501_WARMING_UP = 0,
  BSP_SR501_READY
} BspSr501Status;

typedef struct {
  BspSr501Status status;
  bool raw_high;
  bool motion_detected;
  uint32_t event_count;
  uint32_t last_motion_ms;
  uint32_t warmup_remaining_ms;
} BspSr501Snapshot;

void BspSr501_Init(uint32_t now_ms);
void BspSr501_Run(uint32_t now_ms);
void BspSr501_GetSnapshot(BspSr501Snapshot *snapshot);

#endif
