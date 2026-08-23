#ifndef CHASSIS_ICM45686_STM32_PRIVATE_H
#define CHASSIS_ICM45686_STM32_PRIVATE_H

#include "components/icm45686/icm45686.h"
#include "drivers/sensor/icm45686.h"
#include "stm32g4xx_hal.h"

#define ICM45686_STM32_FIFO_BATCH_FRAMES 4U
#define ICM45686_STM32_FIFO_TRANSFER_SIZE                               \
  (1U + ICM45686_STM32_FIFO_BATCH_FRAMES * ICM45686_FIFO_FRAME_SIZE)

typedef enum {
  ICM45686_DMA_IDLE = 0,
  ICM45686_DMA_BUSY,
  ICM45686_DMA_COMPLETE,
  ICM45686_DMA_ERROR
} Icm45686DmaState;

typedef struct {
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
} Icm45686Stm32Config;

typedef struct {
  Icm45686Device chip;
  Icm45686Stm32Snapshot snapshot;
  Icm45686Stm32SampleSink sample_sink;
  volatile bool fifo_interrupt;
  volatile uint32_t interrupt_count;
  volatile Icm45686DmaState dma_state;
  uint8_t dma_tx[ICM45686_STM32_FIFO_TRANSFER_SIZE];
  uint8_t dma_rx[ICM45686_STM32_FIFO_TRANSFER_SIZE];
  uint16_t dma_frame_count;
  uint16_t dma_remaining_frame_count;
  bool drain_pending;
  uint32_t dma_started_ms;
  uint32_t ready_after_ms;
  uint32_t last_attempt_ms;
  uint32_t last_fifo_poll_ms;
  uint32_t consecutive_errors;
  uint16_t last_fifo_timestamp;
  bool fifo_timestamp_valid;
} Icm45686Stm32Data;

extern const SensorDriverApi icm45686_stm32_api;
int Icm45686Device_Init(const struct device *device);

#endif
