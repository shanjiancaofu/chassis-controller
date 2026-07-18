#include "fdcan_driver.h"

#include <string.h>

#include "fdcan.h"

#define LOOPBACK_CAN_ID 0x321U

static volatile FdcanLoopbackStatus loopback_status = FDCAN_LOOPBACK_PENDING;

HAL_StatusTypeDef FdcanDriver_Init(void)
{
  FDCAN_FilterTypeDef filter = {0};
  FDCAN_TxHeaderTypeDef tx_header = {0};
  uint8_t tx_data[8] = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U};

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = LOOPBACK_CAN_ID;
  filter.FilterID2 = 0x7FFU;
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

  tx_header.Identifier = LOOPBACK_CAN_ID;
  tx_header.IdType = FDCAN_STANDARD_ID;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  tx_header.DataLength = FDCAN_DLC_BYTES_8;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = FDCAN_BRS_ON;
  tx_header.FDFormat = FDCAN_FD_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker = 0U;
  return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, tx_data);
}

FdcanLoopbackStatus FdcanDriver_GetLoopbackStatus(void)
{
  return loopback_status;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_its)
{
  FDCAN_RxHeaderTypeDef rx_header = {0};
  uint8_t rx_data[8] = {0};
  static const uint8_t expected_data[8] = {'C', 'H', 'A', 'S',
                                           'S', 'I', 'S', 1U};

  if (hfdcan != &hfdcan2 ||
      (rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U) {
    return;
  }

  if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, rx_data) ==
          HAL_OK &&
      rx_header.Identifier == LOOPBACK_CAN_ID &&
      rx_header.DataLength == FDCAN_DLC_BYTES_8 &&
      memcmp(rx_data, expected_data, sizeof(expected_data)) == 0) {
    loopback_status = FDCAN_LOOPBACK_PASSED;
  } else {
    loopback_status = FDCAN_LOOPBACK_FAILED;
  }
}
