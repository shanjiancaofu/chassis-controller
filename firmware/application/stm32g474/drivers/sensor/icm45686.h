#ifndef CHASSIS_ICM45686_DRIVER_H
#define CHASSIS_ICM45686_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

typedef enum {
  ICM45686_UNINITIALIZED = 0,
  ICM45686_NOT_FOUND,
  ICM45686_READY,
  ICM45686_DEGRADED
} Icm45686Stm32Status;

typedef struct {
  float accel_mps2[3];
  float gyro_rad_s[3];
  float sample_period_s;
} Icm45686Stm32Sample;

typedef struct {
  void (*reset)(void);
  void (*process_sample)(const Icm45686Stm32Sample *sample);
} Icm45686Stm32SampleSink;

typedef struct {
  Icm45686Stm32Status status;
  uint8_t who_am_i;
  bool sample_valid;
  bool fifo_enabled;
  bool dma_busy;
  int16_t accel[3];
  int16_t gyro[3];
  int16_t temperature;
  float accel_mps2[3];
  float gyro_rad_s[3];
  float temperature_c;
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
} Icm45686Stm32Snapshot;

void Icm45686Stm32_Init(uint32_t now_ms);
bool Icm45686Stm32_SetSampleSink(const Icm45686Stm32SampleSink *sink);
void Icm45686Stm32_Run(uint32_t now_ms);
void Icm45686Stm32_OnDataReadyInterrupt(void);
void Icm45686Stm32_OnSpiTransferComplete(void);
void Icm45686Stm32_OnSpiTransferError(void);
void Icm45686Stm32_GetSnapshot(Icm45686Stm32Snapshot *snapshot);

typedef struct {
  bool (*set_sample_sink)(const struct device *device,
                          const Icm45686Stm32SampleSink *sink);
  void (*run)(const struct device *device, uint32_t now_ms);
  void (*get_snapshot)(const struct device *device,
                       Icm45686Stm32Snapshot *snapshot);
  void (*on_data_ready)(const struct device *device);
  void (*on_transfer_complete)(const struct device *device);
  void (*on_transfer_error)(const struct device *device);
} SensorDriverApi;

bool sensor_set_sample_sink(const struct device *device,
                            const Icm45686Stm32SampleSink *sink);
void sensor_run(const struct device *device, uint32_t now_ms);
void sensor_get_snapshot(const struct device *device,
                         Icm45686Stm32Snapshot *snapshot);
void sensor_on_data_ready(const struct device *device);
void sensor_on_transfer_complete(const struct device *device);
void sensor_on_transfer_error(const struct device *device);
extern const SensorDriverApi icm45686_stm32_api;
int Icm45686Device_Init(const struct device *device);

#endif
