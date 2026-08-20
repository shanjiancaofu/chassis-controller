#include "communication/can_transport/can_transport.h"

#include <errno.h>
#include <string.h>

#include "drivers/time.h"
#include "config/control_config.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "drivers/can.h"
#include "../../../../shared/ota_protocol.h"

#define CAN_HANDSHAKE_REQUEST_ID 0x720U
#define CAN_HANDSHAKE_RESPONSE_ID 0x721U
#define CAN_CONTROL_COMMAND_ID 0x100U
#define CAN_CONTROL_DATA_SIZE 8U
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
static const struct device *can_device;

static void HandleHandshakeFrame(const uint8_t data[CAN_CONTROL_DATA_SIZE]);
static void HandleControlFrame(const uint8_t data[CAN_CONTROL_DATA_SIZE]);
static void ProcessRxFrame(const struct can_frame *frame);
static void InvalidateControlSession(CanTransportLinkStatus status);
static void ResetControlSession(void);
static bool TakeResponseRequest(void);

int CanTransport_Init(const struct device *device)
{
  static const struct can_filter accepted_filters[] = {
      {.id = CAN_HANDSHAKE_REQUEST_ID, .mask = 0x7FFU},
      {.id = CAN_CONTROL_COMMAND_ID, .mask = 0x7FFU},
      {.id = OTA_CAN_REQUEST_ID, .mask = 0x7FFU},
  };

  if (!device_is_ready(device)) {
    return -ENODEV;
  }
  can_device = device;

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
  return can_start(can_device, accepted_filters,
                   sizeof(accepted_filters) / sizeof(accepted_filters[0]));
}

void CanTransport_Run(void)
{
  static const struct can_frame response = {
      .id = CAN_HANDSHAKE_RESPONSE_ID,
      .dlc = CAN_CONTROL_DATA_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
      .data = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U},
  };

  const uint32_t now_ms = time_uptime_ms();
  struct can_frame frame;
  const uint32_t events = can_take_error_events(can_device);

  while (can_recv(can_device, &frame) == 0) {
    ProcessRxFrame(&frame);
  }

  if ((events & (CAN_ERROR_PASSIVE | CAN_ERROR_BUS_OFF |
                 CAN_ERROR_PROTOCOL | CAN_ERROR_RX_FIFO_FULL |
                 CAN_ERROR_RX_FIFO_LOST)) != 0U) {
    InvalidateControlSession(CAN_TRANSPORT_LINK_FAILED);
  }
  if ((events & CAN_ERROR_BUS_OFF) != 0U) {
    recovery_pending = true;
    recovery_failed = false;
    recovery_attempts = 0U;
    recovery_retry_due_ms = now_ms + CAN_RECOVERY_RETRY_DELAY_MS;
  }

  if (recovery_pending &&
      (int32_t)(now_ms - recovery_retry_due_ms) >= 0) {
    ++recovery_attempts;
    if (can_recover(can_device) == 0) {
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

  if (can_send(can_device, &response) < 0) {
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
  struct can_diagnostics hardware = {0};

  if (diagnostics == NULL ||
      can_get_diagnostics(can_device, &hardware) < 0) {
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

static void ProcessRxFrame(const struct can_frame *frame)
{
  if (frame == NULL) {
    return;
  }
  if (frame->id == CAN_HANDSHAKE_REQUEST_ID &&
      frame->dlc == CAN_CONTROL_DATA_SIZE) {
    HandleHandshakeFrame(frame->data);
  } else if (frame->id == CAN_CONTROL_COMMAND_ID &&
             frame->dlc == CAN_CONTROL_DATA_SIZE) {
    HandleControlFrame(frame->data);
  } else if (frame->id == OTA_CAN_REQUEST_ID) {
    (void)OtaCanTransport_OnRxFrame(frame);
  }
}

static void HandleHandshakeFrame(const uint8_t data[CAN_CONTROL_DATA_SIZE])
{
  static const uint8_t request[CAN_CONTROL_DATA_SIZE] = {
      'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U,
  };
  static const uint8_t confirmation[CAN_CONTROL_DATA_SIZE] = {
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

static void HandleControlFrame(const uint8_t data[CAN_CONTROL_DATA_SIZE])
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
