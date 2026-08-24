#include "subsys/communication/can/can_transport.h"

#include <errno.h>

#include "drivers/time.h"

#define CAN_RECOVERY_RETRY_LIMIT 3U
#define CAN_RECOVERY_RETRY_DELAY_MS 100U

static volatile bool session_invalidated;
static volatile bool recovered;
static uint32_t recovery_count;
static uint32_t recovery_failure_count;
static uint32_t recovery_retry_due_ms;
static uint8_t recovery_attempts;
static bool recovery_pending;
static bool recovery_failed;
static const struct device *can_device;

int CanTransport_Init(const struct device *device,
                      const struct can_filter *filters, size_t filter_count) {
  if (!device_is_ready(device) || filters == NULL || filter_count == 0U) {
    return -EINVAL;
  }
  can_device = device;
  session_invalidated = false;
  recovered = false;
  recovery_count = 0U;
  recovery_failure_count = 0U;
  recovery_retry_due_ms = 0U;
  recovery_attempts = 0U;
  recovery_pending = false;
  recovery_failed = false;
  return can_start(can_device, filters, filter_count);
}

void CanTransport_Run(void) {
  const uint32_t now_ms = time_uptime_ms();
  const uint32_t events = can_take_error_events(can_device);

  if ((events & (CAN_ERROR_PASSIVE | CAN_ERROR_BUS_OFF | CAN_ERROR_PROTOCOL |
                 CAN_ERROR_RX_FIFO_FULL | CAN_ERROR_RX_FIFO_LOST)) != 0U) {
    session_invalidated = true;
  }
  if ((events & CAN_ERROR_BUS_OFF) != 0U) {
    recovery_pending = true;
    recovery_failed = false;
    recovery_attempts = 0U;
    recovery_retry_due_ms = now_ms + CAN_RECOVERY_RETRY_DELAY_MS;
  }
  if (recovery_pending && (int32_t)(now_ms - recovery_retry_due_ms) >= 0) {
    ++recovery_attempts;
    if (can_recover(can_device) == 0) {
      ++recovery_count;
      recovered = true;
      recovery_pending = false;
      recovery_attempts = 0U;
    } else if (recovery_attempts >= CAN_RECOVERY_RETRY_LIMIT) {
      ++recovery_failure_count;
      recovery_pending = false;
      recovery_failed = true;
    } else {
      recovery_retry_due_ms = now_ms + CAN_RECOVERY_RETRY_DELAY_MS;
    }
  }
}

int CanTransport_Receive(struct can_frame *frame) {
  return can_recv(can_device, frame);
}

int CanTransport_Send(const struct can_frame *frame) {
  return can_send(can_device, frame);
}

bool CanTransport_TakeSessionInvalidated(void) {
  const bool invalidated = session_invalidated;
  session_invalidated = false;
  return invalidated;
}

bool CanTransport_TakeRecovered(void) {
  const bool event = recovered;
  recovered = false;
  return event;
}

bool CanTransport_GetDiagnostics(CanTransportDiagnostics *diagnostics) {
  struct can_diagnostics hardware = {0};

  if (diagnostics == NULL || can_get_diagnostics(can_device, &hardware) < 0) {
    return false;
  }
  diagnostics->last_error_code = hardware.last_error_code;
  diagnostics->data_last_error_code = hardware.data_last_error_code;
  diagnostics->activity = hardware.activity;
  diagnostics->tx_error_count = hardware.tx_error_count;
  diagnostics->rx_error_count = hardware.rx_error_count;
  diagnostics->error_passive = hardware.error_passive;
  diagnostics->warning = hardware.warning;
  diagnostics->bus_off = hardware.bus_off;
  diagnostics->restricted_mode = hardware.restricted_mode;
  diagnostics->rx_fifo_fill = hardware.rx_fifo_fill;
  diagnostics->tx_fifo_free = hardware.tx_fifo_free;
  diagnostics->warning_count = hardware.warning_count;
  diagnostics->error_passive_count = hardware.error_passive_count;
  diagnostics->bus_off_count = hardware.bus_off_count;
  diagnostics->protocol_error_count = hardware.protocol_error_count;
  diagnostics->rx_fifo_full_count = hardware.rx_fifo_full_count;
  diagnostics->rx_fifo_lost_count = hardware.rx_fifo_lost_count;
  diagnostics->recovery_count = recovery_count;
  diagnostics->recovery_failure_count = recovery_failure_count;
  return true;
}

bool CanTransport_IsOperational(void) { return !recovery_failed; }
