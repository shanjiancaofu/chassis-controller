#include "bsp/fdcan/fdcan_bsp.h"

#include <string.h>

#include "fdcan.h"

HAL_StatusTypeDef BspFdcan_Start(const uint32_t *accepted_standard_ids,
                                 size_t accepted_id_count)
{
  FDCAN_FilterTypeDef filter = {0};
  size_t index;

  if (accepted_standard_ids == NULL || accepted_id_count == 0U ||
      accepted_id_count > hfdcan2.Init.StdFiltersNbr) {
    return HAL_ERROR;
  }

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID2 = 0x7FFU;
  for (index = 0U; index < accepted_id_count; ++index) {
    if (accepted_standard_ids[index] > 0x7FFU) {
      return HAL_ERROR;
    }

    filter.FilterIndex = (uint32_t)index;
    filter.FilterID1 = accepted_standard_ids[index];
    if (HAL_FDCAN_ConfigFilter(&hfdcan2, &filter) != HAL_OK) {
      return HAL_ERROR;
    }
  }

  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_FDCAN_ActivateNotification(
          &hfdcan2,
          FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL |
              FDCAN_IT_RX_FIFO0_MESSAGE_LOST | FDCAN_IT_ERROR_WARNING |
              FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_BUS_OFF |
              FDCAN_IT_ARB_PROTOCOL_ERROR |
              FDCAN_IT_DATA_PROTOCOL_ERROR,
          0U) != HAL_OK) {
    return HAL_ERROR;
  }
  return HAL_FDCAN_Start(&hfdcan2);
}

HAL_StatusTypeDef BspFdcan_Restart(void)
{
  if (HAL_FDCAN_Stop(&hfdcan2) != HAL_OK) {
    return HAL_ERROR;
  }
  return HAL_FDCAN_Start(&hfdcan2);
}

HAL_StatusTypeDef BspFdcan_ReadRxFrame(BspFdcanFrame *frame)
{
  FDCAN_RxHeaderTypeDef header = {0};

  if (frame == NULL ||
      HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO0, &header,
                             frame->data) != HAL_OK ||
      header.DataLength != FDCAN_DLC_BYTES_8 ||
      header.FDFormat != FDCAN_FD_CAN ||
      header.BitRateSwitch != FDCAN_BRS_ON) {
    return HAL_ERROR;
  }

  frame->identifier = header.Identifier;
  return HAL_OK;
}

HAL_StatusTypeDef BspFdcan_SendFrame(const BspFdcanFrame *frame)
{
  FDCAN_TxHeaderTypeDef header = {0};
  uint8_t data[BSP_FDCAN_DATA_SIZE];

  if (frame == NULL || frame->identifier > 0x7FFU) {
    return HAL_ERROR;
  }

  memcpy(data, frame->data, sizeof(data));
  header.Identifier = frame->identifier;
  header.IdType = FDCAN_STANDARD_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = FDCAN_DLC_BYTES_8;
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = FDCAN_BRS_ON;
  header.FDFormat = FDCAN_FD_CAN;
  header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  header.MessageMarker = 0U;
  return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &header, data);
}

bool BspFdcan_GetDiagnostics(BspFdcanDiagnostics *diagnostics)
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
