#include "communication/can_transport/can_transport.h"

#include <string.h>

#include "bsp/fdcan/fdcan_bsp.h"
#include "bsp/time/bsp_time.h"
#include "config/control_config.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "../../../../shared/ota_protocol.h"

#define CAN_HANDSHAKE_REQUEST_ID 0x720U
#define CAN_HANDSHAKE_RESPONSE_ID 0x721U
#define CAN_CONTROL_COMMAND_ID 0x100U
#define CAN_RESPONSE_RETRY_LIMIT 3U
#define CAN_RESPONSE_RETRY_DELAY_MS 10U
#define CAN_RECOVERY_RETRY_LIMIT 3U
#define CAN_RECOVERY_RETRY_DELAY_MS 100U

static volatile CanTransportLinkStatus link_status;
static volatile bool response_pending;
static volatile bool session_invalidated;
static volatile bool control_command_pending;
static volatile bool control_sequence_valid;
static volatile uint8_t last_control_sequence;
static volatile CanTransportControlCommand pending_control_command;
static uint32_t recovery_count;
static uint32_t recovery_failure_count;
static uint32_t response_retry_due_ms;
static uint32_t recovery_retry_due_ms;
static uint8_t response_attempts;
static uint8_t recovery_attempts;
static bool recovery_pending;
static bool recovery_failed;

static void HandleHandshakeFrame(
    const uint8_t data[BSP_FDCAN_CONTROL_DATA_SIZE]);
static void HandleControlFrame(
    const uint8_t data[BSP_FDCAN_CONTROL_DATA_SIZE]);
static void ProcessRxFrame(const BspFdcanFrame *frame);
static void InvalidateControlSession(CanTransportLinkStatus status);
static void ResetControlSession(void);
static bool TakeResponseRequest(void);

bool CanTransport_Init(void)
{
  static const uint32_t accepted_ids[] = {
      CAN_HANDSHAKE_REQUEST_ID,
      CAN_CONTROL_COMMAND_ID,
      OTA_CAN_REQUEST_ID,
  };

  link_status = CAN_TRANSPORT_LINK_READY;
  response_pending = false;
  session_invalidated = false;
  recovery_count = 0U;
  recovery_failure_count = 0U;
  response_retry_due_ms = 0U;
  recovery_retry_due_ms = 0U;
  response_attempts = 0U;
  recovery_attempts = 0U;
  recovery_pending = false;
  recovery_failed = false;
  ResetControlSession();
  return BspFdcan_Start(accepted_ids,
                        sizeof(accepted_ids) / sizeof(accepted_ids[0]));
}

void CanTransport_Run(void)
{
  static const BspFdcanFrame response = {
      .identifier = CAN_HANDSHAKE_RESPONSE_ID,
      .length = BSP_FDCAN_CONTROL_DATA_SIZE,
      .data = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U},
  };

  const uint32_t now_ms = BspTime_GetUptimeMs();
  BspFdcanFrame frame;
  const uint32_t events = BspFdcan_TakeErrorEvents();

  while (BspFdcan_TakeRxFrame(&frame)) {
    ProcessRxFrame(&frame);
  }

  if ((events & (BSP_FDCAN_ERROR_PASSIVE | BSP_FDCAN_ERROR_BUS_OFF |
                 BSP_FDCAN_ERROR_PROTOCOL | BSP_FDCAN_ERROR_RX_FIFO_FULL |
                 BSP_FDCAN_ERROR_RX_FIFO_LOST)) != 0U) {
    InvalidateControlSession(CAN_TRANSPORT_LINK_FAILED);
  }
  if ((events & BSP_FDCAN_ERROR_BUS_OFF) != 0U) {
    recovery_pending = true;
    recovery_failed = false;
    recovery_attempts = 0U;
    recovery_retry_due_ms = now_ms + CAN_RECOVERY_RETRY_DELAY_MS;
  }

  if (recovery_pending &&
      (int32_t)(now_ms - recovery_retry_due_ms) >= 0) {
    ++recovery_attempts;
    if (BspFdcan_Restart()) {
      ++recovery_count;
      recovery_pending = false;
      recovery_attempts = 0U;
      link_status = CAN_TRANSPORT_LINK_READY;
      ResetControlSession();
    } else if (recovery_attempts >= CAN_RECOVERY_RETRY_LIMIT) {
      ++recovery_failure_count;
      recovery_pending = false;
      recovery_failed = true;
    } else {
      recovery_retry_due_ms = now_ms + CAN_RECOVERY_RETRY_DELAY_MS;
    }
  }

  if ((int32_t)(now_ms - response_retry_due_ms) < 0 ||
      !TakeResponseRequest()) {
    return;
  }

  if (!BspFdcan_SendFrame(&response)) {
    ++response_attempts;
    if (response_attempts < CAN_RESPONSE_RETRY_LIMIT) {
      response_retry_due_ms = now_ms + CAN_RESPONSE_RETRY_DELAY_MS;
      response_pending = true;
    } else {
      response_attempts = 0U;
      InvalidateControlSession(CAN_TRANSPORT_LINK_FAILED);
    }
  } else {
    response_attempts = 0U;
  }
}

CanTransportLinkStatus CanTransport_GetLinkStatus(void)
{
  return link_status;
}

bool CanTransport_TakeControlCommand(CanTransportControlCommand *command)
{
  if (command == NULL) {
    return false;
  }
  if (!control_command_pending) {
    return false;
  }

  *command = pending_control_command;
  control_command_pending = false;
  return true;
}

bool CanTransport_TakeSessionInvalidated(void)
{
  const bool invalidated = session_invalidated;

  session_invalidated = false;
  return invalidated;
}

bool CanTransport_GetDiagnostics(CanTransportDiagnostics *diagnostics)
{
  BspFdcanDiagnostics hardware = {0};
  BspFdcanEventCounters counters = {0};

  if (diagnostics == NULL || !BspFdcan_GetDiagnostics(&hardware)) {
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
  BspFdcan_GetEventCounters(&counters);
  diagnostics->warning_count = counters.warning_count;
  diagnostics->error_passive_count = counters.error_passive_count;
  diagnostics->bus_off_count = counters.bus_off_count;
  diagnostics->protocol_error_count = counters.protocol_error_count;
  diagnostics->rx_fifo_full_count = counters.rx_fifo_full_count;
  diagnostics->rx_fifo_lost_count = counters.rx_fifo_lost_count;
  diagnostics->recovery_count = recovery_count;
  diagnostics->recovery_failure_count = recovery_failure_count;
  return true;
}

bool CanTransport_IsOperational(void)
{
  return !recovery_failed;
}

void CanTransport_RequestResponse(void)
{
  response_attempts = 0U;
  response_retry_due_ms = 0U;
  response_pending = true;
}

static void ProcessRxFrame(const BspFdcanFrame *frame)
{
  if (frame == NULL) {
    return;
  }
  if (frame->identifier == CAN_HANDSHAKE_REQUEST_ID &&
      frame->length == BSP_FDCAN_CONTROL_DATA_SIZE) {
    HandleHandshakeFrame(frame->data);
  } else if (frame->identifier == CAN_CONTROL_COMMAND_ID &&
             frame->length == BSP_FDCAN_CONTROL_DATA_SIZE) {
    HandleControlFrame(frame->data);
  } else if (frame->identifier == OTA_CAN_REQUEST_ID) {
    (void)OtaCanTransport_OnRxFrame(frame);
  }
}

static void HandleHandshakeFrame(
    const uint8_t data[BSP_FDCAN_CONTROL_DATA_SIZE])
{
  static const uint8_t request[BSP_FDCAN_CONTROL_DATA_SIZE] = {
      'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U,
  };
  static const uint8_t confirmation[BSP_FDCAN_CONTROL_DATA_SIZE] = {
      'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U,
  };

  if (memcmp(data, request, sizeof(request)) == 0) {
    InvalidateControlSession(CAN_TRANSPORT_LINK_READY);
    response_pending = true;
    return;
  }

  if (memcmp(data, confirmation, sizeof(confirmation)) == 0) {
    link_status = CAN_TRANSPORT_LINK_PASSED;
    ResetControlSession();
    response_pending = false;
    return;
  }

  InvalidateControlSession(CAN_TRANSPORT_LINK_FAILED);
  response_pending = false;
}

static void HandleControlFrame(
    const uint8_t data[BSP_FDCAN_CONTROL_DATA_SIZE])
{
  int16_t left_target;
  int16_t right_target;

  if (link_status != CAN_TRANSPORT_LINK_PASSED || data[0] != 1U ||
      (data[1] & 0xFEU) != 0U || data[3] != 0U) {
    return;
  }

  left_target = (int16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8U));
  right_target = (int16_t)((uint16_t)data[6] | ((uint16_t)data[7] << 8U));
  if (left_target > MOTOR_CONTROL_TARGET_LIMIT ||
      left_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      right_target > MOTOR_CONTROL_TARGET_LIMIT ||
      right_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      (data[1] == 0U && (left_target != 0 || right_target != 0)) ||
      (control_sequence_valid &&
       data[2] != (uint8_t)(last_control_sequence + 1U))) {
    return;
  }

  last_control_sequence = data[2];
  control_sequence_valid = true;
  pending_control_command.enabled = data[1] != 0U;
  pending_control_command.sequence = data[2];
  pending_control_command.left_target = left_target;
  pending_control_command.right_target = right_target;
  control_command_pending = true;
}

static void ResetControlSession(void)
{
  control_command_pending = false;
  control_sequence_valid = false;
  last_control_sequence = 0U;
  pending_control_command = (CanTransportControlCommand){0};
}

static void InvalidateControlSession(CanTransportLinkStatus status)
{
  link_status = status;
  ResetControlSession();
  session_invalidated = true;
}

static bool TakeResponseRequest(void)
{
  const bool pending = response_pending;

  response_pending = false;
  return pending;
}
