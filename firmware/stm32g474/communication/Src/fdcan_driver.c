#include "fdcan_driver.h"

#include <stdbool.h>
#include <string.h>

#include "fdcan.h"
#include "project_config.h"

#define CONTROL_COMMAND_ID 0x100U
#define EXTERNAL_TEST_REQUEST_ID 0x720U
#define EXTERNAL_TEST_RESPONSE_ID 0x721U

static volatile FdcanLinkStatus link_status = FDCAN_LINK_READY;
static volatile bool response_pending;
static volatile bool control_command_pending;
static volatile bool control_sequence_valid;
static volatile uint8_t last_control_sequence;
static volatile FdcanControlCommand pending_control_command;

HAL_StatusTypeDef FdcanDriver_Init(void)
{
  FDCAN_FilterTypeDef filter = {0};

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = EXTERNAL_TEST_REQUEST_ID;
  filter.FilterID2 = 0x7FFU;
  if (HAL_FDCAN_ConfigFilter(&hfdcan2, &filter) != HAL_OK) {
    return HAL_ERROR;
  }
  filter.FilterIndex = 1;
  filter.FilterID1 = CONTROL_COMMAND_ID;
  if (HAL_FDCAN_ConfigFilter(&hfdcan2, &filter) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                     0U) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK) {
    return HAL_ERROR;
  }
  return HAL_OK;
}

void FdcanDriver_Run(void)
{
  FDCAN_TxHeaderTypeDef tx_header = {0};
  uint8_t tx_data[8] = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U};

  if (!response_pending) {
    return;
  }

  tx_header.Identifier = EXTERNAL_TEST_RESPONSE_ID;
  tx_header.IdType = FDCAN_STANDARD_ID;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  tx_header.DataLength = FDCAN_DLC_BYTES_8;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = FDCAN_BRS_ON;
  tx_header.FDFormat = FDCAN_FD_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker = 0U;
  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, tx_data) ==
      HAL_OK) {
    response_pending = false;
  } else {
    link_status = FDCAN_LINK_FAILED;
  }
}

FdcanLinkStatus FdcanDriver_GetLinkStatus(void)
{
  return link_status;
}

bool FdcanDriver_TakeControlCommand(FdcanControlCommand *command)
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
  command->enabled = pending_control_command.enabled;
  command->sequence = pending_control_command.sequence;
  command->left_target = pending_control_command.left_target;
  command->right_target = pending_control_command.right_target;
  control_command_pending = false;
  __set_PRIMASK(primask);
  return true;
}

bool FdcanDriver_GetDiagnostics(FdcanDiagnostics *diagnostics)
{
  FDCAN_ProtocolStatusTypeDef protocol_status = {0};
  FDCAN_ErrorCountersTypeDef error_counters = {0};

  if (diagnostics == NULL ||
      HAL_FDCAN_GetProtocolStatus(&hfdcan2, &protocol_status) != HAL_OK ||
      HAL_FDCAN_GetErrorCounters(&hfdcan2, &error_counters) != HAL_OK) {
    return false;
  }

  diagnostics->last_error_code = protocol_status.LastErrorCode;
  diagnostics->data_last_error_code = protocol_status.DataLastErrorCode;
  diagnostics->activity = protocol_status.Activity;
  diagnostics->tx_error_count = error_counters.TxErrorCnt;
  diagnostics->rx_error_count = error_counters.RxErrorCnt;
  diagnostics->error_passive = protocol_status.ErrorPassive;
  diagnostics->warning = protocol_status.Warning;
  diagnostics->bus_off = protocol_status.BusOff;
  diagnostics->restricted_mode =
      HAL_FDCAN_IsRestrictedOperationMode(&hfdcan2);
  diagnostics->rx_fifo_fill =
      HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO0);
  diagnostics->tx_fifo_free = HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan2);
  return true;
}

void FdcanDriver_RequestDiagnosticTransmit(void)
{
  response_pending = true;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_its)
{
  FDCAN_RxHeaderTypeDef rx_header = {0};
  uint8_t rx_data[8] = {0};
  int16_t left_target;
  int16_t right_target;
  static const uint8_t request_data[8] = {'P', 'I', 'N', 'G',
                                          1U,  0U,  0U,  0U};
  static const uint8_t confirm_data[8] = {'P', 'A', 'S', 'S',
                                          1U,  0U,  0U,  0U};

  if (hfdcan != &hfdcan2 ||
      (rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U) {
    return;
  }

  if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, rx_data) !=
          HAL_OK ||
      rx_header.DataLength != FDCAN_DLC_BYTES_8 ||
      rx_header.FDFormat != FDCAN_FD_CAN ||
      rx_header.BitRateSwitch != FDCAN_BRS_ON) {
    return;
  }

  if (rx_header.Identifier == EXTERNAL_TEST_REQUEST_ID) {
    if (memcmp(rx_data, request_data, sizeof(request_data)) == 0) {
      response_pending = true;
    } else if (memcmp(rx_data, confirm_data, sizeof(confirm_data)) == 0) {
      link_status = FDCAN_LINK_PASSED;
      control_sequence_valid = false;
    } else {
      link_status = FDCAN_LINK_FAILED;
    }
    return;
  }

  if (rx_header.Identifier != CONTROL_COMMAND_ID ||
      link_status != FDCAN_LINK_PASSED || rx_data[0] != 1U ||
      (rx_data[1] & 0xFEU) != 0U || rx_data[3] != 0U) {
    return;
  }

  left_target =
      (int16_t)((uint16_t)rx_data[4] | ((uint16_t)rx_data[5] << 8U));
  right_target =
      (int16_t)((uint16_t)rx_data[6] | ((uint16_t)rx_data[7] << 8U));
  if (left_target > MOTOR_CONTROL_TARGET_LIMIT ||
      left_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      right_target > MOTOR_CONTROL_TARGET_LIMIT ||
      right_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      (rx_data[1] == 0U && (left_target != 0 || right_target != 0)) ||
      (control_sequence_valid &&
       rx_data[2] != (uint8_t)(last_control_sequence + 1U))) {
    return;
  }

  last_control_sequence = rx_data[2];
  control_sequence_valid = true;
  pending_control_command.enabled = rx_data[1] != 0U;
  pending_control_command.sequence = rx_data[2];
  pending_control_command.left_target = left_target;
  pending_control_command.right_target = right_target;
  control_command_pending = true;
}
