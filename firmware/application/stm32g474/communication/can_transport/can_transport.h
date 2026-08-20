#ifndef CAN_TRANSPORT_H
#define CAN_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

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
  uint32_t warning_count;
  uint32_t error_passive_count;
  uint32_t bus_off_count;
  uint32_t protocol_error_count;
  uint32_t rx_fifo_full_count;
  uint32_t rx_fifo_lost_count;
  uint32_t recovery_count;
  uint32_t recovery_failure_count;
} CanTransportDiagnostics;

bool CanTransport_Init(void);
void CanTransport_Run(void);
CanTransportLinkStatus CanTransport_GetLinkStatus(void);
bool CanTransport_TakeControlCommand(CanTransportControlCommand *command);
bool CanTransport_TakeSessionInvalidated(void);
bool CanTransport_GetDiagnostics(CanTransportDiagnostics *diagnostics);
bool CanTransport_IsOperational(void);
void CanTransport_RequestResponse(void);

#endif
