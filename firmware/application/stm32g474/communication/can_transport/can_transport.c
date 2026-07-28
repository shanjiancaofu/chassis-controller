#include "communication/can_transport/can_transport.h"

#include <string.h>

#include "bsp/fdcan/fdcan_bsp.h"
#include "config/control_config.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "fdcan.h"
#include "../../../../shared/ota_protocol.h"

#define CAN_HANDSHAKE_REQUEST_ID 0x720U
#define CAN_HANDSHAKE_RESPONSE_ID 0x721U
#define CAN_CONTROL_COMMAND_ID 0x100U
#define CAN_RESPONSE_RETRY_LIMIT 3U
#define CAN_RESPONSE_RETRY_DELAY_MS 10U
#define CAN_RECOVERY_RETRY_LIMIT 3U
#define CAN_RECOVERY_RETRY_DELAY_MS 100U

enum {
  CAN_ERROR_EVENT_WARNING = 1U << 0,
  CAN_ERROR_EVENT_PASSIVE = 1U << 1,
  CAN_ERROR_EVENT_BUS_OFF = 1U << 2,
  CAN_ERROR_EVENT_PROTOCOL = 1U << 3,
  CAN_ERROR_EVENT_RX_FIFO_FULL = 1U << 4,
  CAN_ERROR_EVENT_RX_FIFO_LOST = 1U << 5
};

static volatile CanTransportLinkStatus link_status;
static volatile bool response_pending;
static volatile bool session_invalidated;
static volatile bool control_command_pending;
static volatile bool control_sequence_valid;
static volatile uint8_t last_control_sequence;
static volatile CanTransportControlCommand pending_control_command;
static volatile uint32_t error_events;
static volatile uint32_t warning_count;
static volatile uint32_t error_passive_count;
static volatile uint32_t bus_off_count;
static volatile uint32_t protocol_error_count;
static volatile uint32_t rx_fifo_full_count;
static volatile uint32_t rx_fifo_lost_count;
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
static void InvalidateControlSession(CanTransportLinkStatus status);
static void ResetControlSession(void);
static uint32_t TakeErrorEvents(void);
static bool TakeResponseRequest(void);

HAL_StatusTypeDef CanTransport_Init(void)
{
  static const uint32_t accepted_ids[] = {
      CAN_HANDSHAKE_REQUEST_ID,
      CAN_CONTROL_COMMAND_ID,
      OTA_CAN_REQUEST_ID,
  };

  link_status = CAN_TRANSPORT_LINK_READY;
  response_pending = false;
  session_invalidated = false;
  error_events = 0U;
  warning_count = 0U;
  error_passive_count = 0U;
  bus_off_count = 0U;
  protocol_error_count = 0U;
  rx_fifo_full_count = 0U;
  rx_fifo_lost_count = 0U;
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

  const uint32_t now_ms = HAL_GetTick();
  const uint32_t events = TakeErrorEvents();

  if ((events & (CAN_ERROR_EVENT_PASSIVE | CAN_ERROR_EVENT_BUS_OFF |
                 CAN_ERROR_EVENT_PROTOCOL | CAN_ERROR_EVENT_RX_FIFO_FULL |
                 CAN_ERROR_EVENT_RX_FIFO_LOST)) != 0U) {
    InvalidateControlSession(CAN_TRANSPORT_LINK_FAILED);
  }
  if ((events & CAN_ERROR_EVENT_BUS_OFF) != 0U) {
    recovery_pending = true;
    recovery_failed = false;
    recovery_attempts = 0U;
    recovery_retry_due_ms = now_ms + CAN_RECOVERY_RETRY_DELAY_MS;
  }

  if (recovery_pending &&
      (int32_t)(now_ms - recovery_retry_due_ms) >= 0) {
    ++recovery_attempts;
    if (BspFdcan_Restart() == HAL_OK) {
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

  if (BspFdcan_SendFrame(&response) != HAL_OK) {
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
  uint32_t primask;

  if (command == NULL) {
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (!control_command_pending) {
    __set_PRIMASK(primask);
    return false;
  }

  *command = pending_control_command;
  control_command_pending = false;
  __set_PRIMASK(primask);
  return true;
}

bool CanTransport_TakeSessionInvalidated(void)
{
  uint32_t primask = __get_PRIMASK();
  bool invalidated;

  __disable_irq();
  invalidated = session_invalidated;
  session_invalidated = false;
  __set_PRIMASK(primask);
  return invalidated;
}

bool CanTransport_GetDiagnostics(CanTransportDiagnostics *diagnostics)
{
  BspFdcanDiagnostics hardware = {0};

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
  diagnostics->warning_count =
      __atomic_load_n(&warning_count, __ATOMIC_RELAXED);
  diagnostics->error_passive_count =
      __atomic_load_n(&error_passive_count, __ATOMIC_RELAXED);
  diagnostics->bus_off_count =
      __atomic_load_n(&bus_off_count, __ATOMIC_RELAXED);
  diagnostics->protocol_error_count =
      __atomic_load_n(&protocol_error_count, __ATOMIC_RELAXED);
  diagnostics->rx_fifo_full_count =
      __atomic_load_n(&rx_fifo_full_count, __ATOMIC_RELAXED);
  diagnostics->rx_fifo_lost_count =
      __atomic_load_n(&rx_fifo_lost_count, __ATOMIC_RELAXED);
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

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_its)
{
  BspFdcanFrame frame = {0};

  if (hfdcan != &hfdcan2) {
    return;
  }
  if ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_FULL) != 0U) {
    (void)__atomic_fetch_add(&rx_fifo_full_count, 1U, __ATOMIC_RELAXED);
    (void)__atomic_fetch_or(&error_events, CAN_ERROR_EVENT_RX_FIFO_FULL,
                            __ATOMIC_RELAXED);
  }
  if ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != 0U) {
    (void)__atomic_fetch_add(&rx_fifo_lost_count, 1U, __ATOMIC_RELAXED);
    (void)__atomic_fetch_or(&error_events, CAN_ERROR_EVENT_RX_FIFO_LOST,
                            __ATOMIC_RELAXED);
  }
  if ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U ||
      BspFdcan_ReadRxFrame(&frame) != HAL_OK) {
    return;
  }

  if (frame.identifier == CAN_HANDSHAKE_REQUEST_ID &&
      frame.length == BSP_FDCAN_CONTROL_DATA_SIZE) {
    HandleHandshakeFrame(frame.data);
  } else if (frame.identifier == CAN_CONTROL_COMMAND_ID &&
             frame.length == BSP_FDCAN_CONTROL_DATA_SIZE) {
    HandleControlFrame(frame.data);
  } else if (frame.identifier == OTA_CAN_REQUEST_ID) {
    (void)OtaCanTransport_OnRxFrameFromIsr(&frame);
  }
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan,
                                   uint32_t error_status_its)
{
  uint32_t events = 0U;

  if (hfdcan != &hfdcan2) {
    return;
  }
  if ((error_status_its & FDCAN_IT_ERROR_WARNING) != 0U) {
    (void)__atomic_fetch_add(&warning_count, 1U, __ATOMIC_RELAXED);
    events |= CAN_ERROR_EVENT_WARNING;
  }
  if ((error_status_its & FDCAN_IT_ERROR_PASSIVE) != 0U) {
    (void)__atomic_fetch_add(&error_passive_count, 1U, __ATOMIC_RELAXED);
    events |= CAN_ERROR_EVENT_PASSIVE;
  }
  if ((error_status_its & FDCAN_IT_BUS_OFF) != 0U) {
    (void)__atomic_fetch_add(&bus_off_count, 1U, __ATOMIC_RELAXED);
    events |= CAN_ERROR_EVENT_BUS_OFF;
  }
  if ((error_status_its &
       (FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_DATA_PROTOCOL_ERROR)) !=
      0U) {
    (void)__atomic_fetch_add(&protocol_error_count, 1U, __ATOMIC_RELAXED);
    events |= CAN_ERROR_EVENT_PROTOCOL;
  }
  (void)__atomic_fetch_or(&error_events, events, __ATOMIC_RELAXED);
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
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  link_status = status;
  ResetControlSession();
  session_invalidated = true;
  __set_PRIMASK(primask);
}

static uint32_t TakeErrorEvents(void)
{
  return __atomic_exchange_n(&error_events, 0U, __ATOMIC_RELAXED);
}

static bool TakeResponseRequest(void)
{
  uint32_t primask = __get_PRIMASK();
  bool pending;

  __disable_irq();
  pending = response_pending;
  response_pending = false;
  __set_PRIMASK(primask);
  return pending;
}
