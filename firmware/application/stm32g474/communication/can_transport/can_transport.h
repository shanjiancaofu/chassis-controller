#ifndef CAN_TRANSPORT_H
#define CAN_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

typedef enum {
  CAN_TRANSPORT_LINK_READY = 0,
  CAN_TRANSPORT_LINK_PASSED,
  CAN_TRANSPORT_LINK_FAILED
} CanTransportLinkStatus;

typedef struct {
  bool enabled;
  uint8_t sequence;
  int16_t left_target;
  int16_t right_target;
} CanTransportControlCommand;

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
} CanTransportDiagnostics;

HAL_StatusTypeDef CanTransport_Init(void);
void CanTransport_Run(void);
CanTransportLinkStatus CanTransport_GetLinkStatus(void);
bool CanTransport_TakeControlCommand(CanTransportControlCommand *command);
bool CanTransport_GetDiagnostics(CanTransportDiagnostics *diagnostics);
void CanTransport_RequestResponse(void);

#endif
