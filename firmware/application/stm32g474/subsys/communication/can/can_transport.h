#ifndef CAN_TRANSPORT_H
#define CAN_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device.h"
#include "drivers/can.h"

typedef struct {
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
  uint32_t recovery_count;
  uint32_t recovery_failure_count;
} CanTransportDiagnostics;

int CanTransport_Init(const struct device *device,
                      const struct can_filter *filters, size_t filter_count);
void CanTransport_Run(void);
int CanTransport_Receive(struct can_frame *frame);
int CanTransport_Send(const struct can_frame *frame);
bool CanTransport_TakeSessionInvalidated(void);
bool CanTransport_TakeRecovered(void);
bool CanTransport_GetDiagnostics(CanTransportDiagnostics *diagnostics);
bool CanTransport_IsOperational(void);

#endif
