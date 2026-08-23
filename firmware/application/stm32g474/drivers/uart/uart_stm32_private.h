#ifndef CHASSIS_UART_STM32_PRIVATE_H
#define CHASSIS_UART_STM32_PRIVATE_H

#include "drivers/uart.h"
#include "stm32g4xx_hal.h"

#define UART_STM32_DMA_RX_BUFFER_SIZE 128U
#define UART_STM32_RX_RING_SIZE 1024U
#define UART_STM32_TX_QUEUE_DEPTH 8U

typedef struct {
  uint16_t length;
  uint32_t token;
  bool tracked;
  uint8_t data[UART_MAX_WRITE_SIZE];
} UartStm32TxMessage;

typedef struct {
  UART_HandleTypeDef *handle;
} UartStm32Config;

typedef struct {
  uint8_t dma_rx_buffer[UART_STM32_DMA_RX_BUFFER_SIZE];
  uint16_t dma_rx_position;
  uint8_t rx_ring[UART_STM32_RX_RING_SIZE];
  volatile uint16_t rx_head;
  volatile uint16_t rx_tail;
  volatile uint32_t rx_overflow_count;
  volatile uint32_t rx_error_count;
  volatile uint32_t rx_restart_count;
  volatile bool rx_restart_pending;
  UartStm32TxMessage tx_queue[UART_STM32_TX_QUEUE_DEPTH];
  volatile uint8_t tx_head;
  uint8_t tx_tail;
  volatile uint8_t tx_count;
  volatile bool tx_active;
  volatile uint32_t tx_queue_full_count;
  volatile uint32_t tx_error_count;
  uint32_t next_tx_token;
  volatile uint32_t completed_tx_token;
  volatile bool completed_tx_success;
} UartStm32Data;

extern const struct uart_driver_api uart_stm32_api;
int UartStm32_Init(const struct device *device);

#endif
