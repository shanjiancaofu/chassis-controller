#ifndef CHASSIS_DRIVERS_CAN_H
#define CHASSIS_DRIVERS_CAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device.h"

#define CAN_MAX_DATA_LENGTH 64U
#define CAN_FRAME_FDF (1U << 0)
#define CAN_FRAME_BRS (1U << 1)

#define CAN_ERROR_WARNING (1UL << 0)
#define CAN_ERROR_PASSIVE (1UL << 1)
#define CAN_ERROR_BUS_OFF (1UL << 2)
#define CAN_ERROR_PROTOCOL (1UL << 3)
#define CAN_ERROR_RX_FIFO_FULL (1UL << 4)
#define CAN_ERROR_RX_FIFO_LOST (1UL << 5)

struct can_frame {
  uint32_t id;
  uint8_t dlc;
  uint8_t flags;
  uint8_t data[CAN_MAX_DATA_LENGTH];
};

struct can_filter {
  uint32_t id;
  uint32_t mask;
};

struct can_diagnostics {
  uint32_t last_error_code;
  uint32_t data_last_error_code;
  uint32_t activity;
  uint32_t tx_error_count;
  uint32_t rx_error_count;
  uint32_t error_passive;
  uint32_t warning;
  uint32_t bus_off;
  uint32_t restricted_mode;
  uint32_t rx_fifo_fill;
  uint32_t tx_fifo_free;
  uint32_t warning_count;
  uint32_t error_passive_count;
  uint32_t bus_off_count;
  uint32_t protocol_error_count;
  uint32_t rx_fifo_full_count;
  uint32_t rx_fifo_lost_count;
};

struct can_driver_api {
  int (*start)(const struct device *device, const struct can_filter *filters,
               size_t filter_count);
  int (*stop)(const struct device *device);
  int (*send)(const struct device *device, const struct can_frame *frame);
  int (*recv)(const struct device *device, struct can_frame *frame);
  int (*recover)(const struct device *device);
  int (*get_diagnostics)(const struct device *device,
                         struct can_diagnostics *diagnostics);
  uint32_t (*take_error_events)(const struct device *device);
  bool (*is_tx_idle)(const struct device *device);
};

int can_start(const struct device *device, const struct can_filter *filters,
              size_t filter_count);
int can_stop(const struct device *device);
int can_send(const struct device *device, const struct can_frame *frame);
int can_recv(const struct device *device, struct can_frame *frame);
int can_recover(const struct device *device);
int can_get_diagnostics(const struct device *device,
                        struct can_diagnostics *diagnostics);
uint32_t can_take_error_events(const struct device *device);
bool can_is_tx_idle(const struct device *device);

#endif
