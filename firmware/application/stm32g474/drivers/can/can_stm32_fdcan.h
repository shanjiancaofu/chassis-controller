#ifndef CAN_STM32_FDCAN_H
#define CAN_STM32_FDCAN_H

#include "drivers/can.h"
#include "stm32g4xx_hal.h"

typedef struct {
  FDCAN_HandleTypeDef *handle;
  uint8_t filter_capacity;
} CanStm32FdcanConfig;

typedef struct {
  struct can_frame rx_queue[CONFIG_CAN_RX_QUEUE_SIZE];
  volatile uint8_t rx_head;
  volatile uint8_t rx_tail;
  volatile uint8_t rx_count;
  volatile uint32_t error_events;
  volatile uint32_t warning_count;
  volatile uint32_t error_passive_count;
  volatile uint32_t bus_off_count;
  volatile uint32_t protocol_error_count;
  volatile uint32_t rx_fifo_full_count;
  volatile uint32_t rx_fifo_lost_count;
} CanStm32FdcanData;

struct can_stm32_fdcan_driver_api {
  struct can_driver_api common;
  int (*send_with_tx_event)(const struct device *device,
                            const struct can_frame *frame, uint8_t token);
  int (*take_tx_event)(const struct device *device, uint8_t *token);
};

int CanStm32Fdcan_Init(const struct device *device);
int can_stm32_fdcan_send_with_tx_event(const struct device *device,
                                       const struct can_frame *frame,
                                       uint8_t token);
int can_stm32_fdcan_take_tx_event(const struct device *device,
                                  uint8_t *token);
extern const struct can_stm32_fdcan_driver_api can_stm32_fdcan_api;

#endif
