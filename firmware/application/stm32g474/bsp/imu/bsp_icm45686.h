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
  bool fifo_enabled;
  bool dma_busy;
  bool calibrated;
  bool orientation_valid;
  uint16_t calibration_samples;
  int16_t accel[3];
  int16_t gyro[3];
  int16_t temperature;
  float accel_mps2[3];
  float gyro_rad_s[3];
  float temperature_c;
  float gyro_bias_rad_s[3];
  float quaternion[4];
  float roll_rad;
  float pitch_rad;
  float yaw_rad;
  bool kalman_valid;
  float kalman_roll_rad;
  float kalman_pitch_rad;
  uint32_t last_sample_ms;
  uint16_t fifo_timestamp;
  uint32_t sample_count;
  uint32_t interrupt_count;
  uint32_t fifo_frame_count;
  uint32_t fifo_parse_error_count;
  uint32_t fifo_full_count;
  uint32_t fifo_flush_count;
  uint32_t fifo_flush_error_count;
  uint32_t timestamp_error_count;
  uint32_t dma_timeout_count;
  uint32_t transfer_error_count;
  float sample_period_s;
} BspIcm45686Snapshot;

void BspIcm45686_Init(uint32_t now_ms);
void BspIcm45686_Run(uint32_t now_ms);
void BspIcm45686_OnDataReadyInterrupt(void);
void BspIcm45686_OnSpiTransferComplete(void);
void BspIcm45686_OnSpiTransferError(void);
void BspIcm45686_GetSnapshot(BspIcm45686Snapshot *snapshot);

#endif
