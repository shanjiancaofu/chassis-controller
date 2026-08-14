#ifndef BSP_ICM45686_H
#define BSP_ICM45686_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BSP_ICM45686_UNINITIALIZED = 0,
  BSP_ICM45686_NOT_FOUND,
  BSP_ICM45686_READY,
  BSP_ICM45686_DEGRADED
} BspIcm45686Status;

typedef struct {
  BspIcm45686Status status;
  uint8_t who_am_i;
  bool sample_valid;
  int16_t accel[3];
  int16_t gyro[3];
  int16_t temperature;
  uint32_t last_sample_ms;
  uint32_t sample_count;
  uint32_t interrupt_count;
  uint32_t transfer_error_count;
} BspIcm45686Snapshot;

void BspIcm45686_Init(uint32_t now_ms);
void BspIcm45686_Run(uint32_t now_ms);
void BspIcm45686_OnDataReadyInterrupt(void);
void BspIcm45686_GetSnapshot(BspIcm45686Snapshot *snapshot);

#endif
