#include "communication/can_transport/can_transport.h"

#include <string.h>

#include "bsp/fdcan/fdcan_bsp.h"
#include "config/control_config.h"
#include "fdcan.h"

#define CAN_HANDSHAKE_REQUEST_ID 0x720U
#define CAN_HANDSHAKE_RESPONSE_ID 0x721U
#define CAN_CONTROL_COMMAND_ID 0x100U

static volatile CanTransportLinkStatus link_status;
static volatile bool response_pending;
static volatile bool control_command_pending;
static volatile bool control_sequence_valid;
static volatile uint8_t last_control_sequence;
static volatile CanTransportControlCommand pending_control_command;

static void HandleHandshakeFrame(const uint8_t data[BSP_FDCAN_DATA_SIZE]);
static void HandleControlFrame(const uint8_t data[BSP_FDCAN_DATA_SIZE]);
static void ResetControlSession(void);
static bool TakeResponseRequest(void);

HAL_StatusTypeDef CanTransport_Init(void)
{
  static const uint32_t accepted_ids[] = {
      CAN_HANDSHAKE_REQUEST_ID,
      CAN_CONTROL_COMMAND_ID,
  };

  link_status = CAN_TRANSPORT_LINK_READY;
  response_pending = false;
  ResetControlSession();
  return BspFdcan_Start(accepted_ids,
                        sizeof(accepted_ids) / sizeof(accepted_ids[0]));
}

void CanTransport_Run(void)
{
  static const BspFdcanFrame response = {
      .identifier = CAN_HANDSHAKE_RESPONSE_ID,
      .data = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U},
  };

  if (!TakeResponseRequest()) {
    return;
  }

  if (BspFdcan_SendFrame(&response) != HAL_OK) {
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    link_status = CAN_TRANSPORT_LINK_FAILED;
    ResetControlSession();
    __set_PRIMASK(primask);
    CanTransport_RequestResponse();
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
  return true;
}

void CanTransport_RequestResponse(void)
{
  response_pending = true;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_its)
{
  BspFdcanFrame frame = {0};

  if (hfdcan != &hfdcan2 ||
      (rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U ||
      BspFdcan_ReadRxFrame(&frame) != HAL_OK) {
    return;
  }

  if (frame.identifier == CAN_HANDSHAKE_REQUEST_ID) {
    HandleHandshakeFrame(frame.data);
  } else if (frame.identifier == CAN_CONTROL_COMMAND_ID) {
    HandleControlFrame(frame.data);
  }
}

static void HandleHandshakeFrame(const uint8_t data[BSP_FDCAN_DATA_SIZE])
{
  static const uint8_t request[BSP_FDCAN_DATA_SIZE] = {
      'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U,
  };
  static const uint8_t confirmation[BSP_FDCAN_DATA_SIZE] = {
      'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U,
  };

  if (memcmp(data, request, sizeof(request)) == 0) {
    link_status = CAN_TRANSPORT_LINK_READY;
    ResetControlSession();
    response_pending = true;
    return;
  }

  if (memcmp(data, confirmation, sizeof(confirmation)) == 0) {
    link_status = CAN_TRANSPORT_LINK_PASSED;
    ResetControlSession();
    response_pending = false;
    return;
  }

  link_status = CAN_TRANSPORT_LINK_FAILED;
  ResetControlSession();
  response_pending = false;
}

static void HandleControlFrame(const uint8_t data[BSP_FDCAN_DATA_SIZE])
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
