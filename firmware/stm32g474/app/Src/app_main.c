#include "app_main.h"

#include <string.h>

#include "fdcan.h"
#include "main.h"
#include "usart.h"

#define LOOPBACK_CAN_ID 0x321U

void App_Init(void)
{
  static const uint8_t startup_message[] = "chassis-controller started\r\n";
  FDCAN_FilterTypeDef filter = {0};
  FDCAN_TxHeaderTypeDef tx_header = {0};
  uint8_t tx_data[8] = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U};

  HAL_UART_Transmit(&huart1, startup_message, sizeof(startup_message) - 1U,
                    HAL_MAX_DELAY);

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = LOOPBACK_CAN_ID;
  filter.FilterID2 = 0x7FFU;
  if (HAL_FDCAN_ConfigFilter(&hfdcan2, &filter) != HAL_OK ||
      HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK ||
      HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                     0U) != HAL_OK ||
      HAL_FDCAN_Start(&hfdcan2) != HAL_OK) {
    Error_Handler();
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
  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, tx_data) != HAL_OK) {
    Error_Handler();
  }
}

void App_Run(void)
{
  static uint32_t last_heartbeat_ms;
  const uint32_t now_ms = HAL_GetTick();

  if (now_ms - last_heartbeat_ms >= 500U) {
    last_heartbeat_ms = now_ms;
    HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
  }
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
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
  }
}
