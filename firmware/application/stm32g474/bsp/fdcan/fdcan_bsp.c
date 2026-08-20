#include "bsp/fdcan/fdcan_bsp.h"

#include <string.h>

#include "board/board_config.h"

#define BSP_FDCAN_RX_QUEUE_DEPTH 8U

static BspFdcanFrame rx_queue[BSP_FDCAN_RX_QUEUE_DEPTH];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile uint8_t rx_count;
static volatile uint32_t error_events;
static volatile BspFdcanEventCounters event_counters;

static bool LengthToDlc(uint8_t length, uint32_t *dlc);
static uint8_t DlcToLength(uint32_t dlc);
static bool ReadRxFrameFromIsr(BspFdcanFrame *frame);
static bool SendFrame(const BspFdcanFrame *frame, bool store_tx_event,
                      uint8_t message_marker);

bool BspFdcan_Start(const uint32_t *accepted_standard_ids,
                    size_t accepted_id_count)
{
  FDCAN_FilterTypeDef filter = {0};
  size_t index;

  if (accepted_standard_ids == NULL || accepted_id_count == 0U ||
      accepted_id_count > BOARD_FDCAN.Init.StdFiltersNbr * 2U) {
    return false;
  }

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  for (index = 0U; index < accepted_id_count; index += 2U) {
    if (accepted_standard_ids[index] > 0x7FFU ||
        (index + 1U < accepted_id_count &&
         accepted_standard_ids[index + 1U] > 0x7FFU)) {
      return false;
    }

    filter.FilterIndex = (uint32_t)(index / 2U);
    filter.FilterID1 = accepted_standard_ids[index];
    if (index + 1U < accepted_id_count) {
      filter.FilterType = FDCAN_FILTER_DUAL;
      filter.FilterID2 = accepted_standard_ids[index + 1U];
    } else {
      filter.FilterType = FDCAN_FILTER_MASK;
      filter.FilterID2 = 0x7FFU;
    }
    if (HAL_FDCAN_ConfigFilter(&BOARD_FDCAN, &filter) != HAL_OK) {
      return false;
    }
  }

  if (HAL_FDCAN_ConfigGlobalFilter(&BOARD_FDCAN, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK) {
    return false;
  }
  if (HAL_FDCAN_ActivateNotification(
          &BOARD_FDCAN,
          FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL |
              FDCAN_IT_RX_FIFO0_MESSAGE_LOST | FDCAN_IT_ERROR_WARNING |
              FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_BUS_OFF |
              FDCAN_IT_ARB_PROTOCOL_ERROR |
              FDCAN_IT_DATA_PROTOCOL_ERROR,
          0U) != HAL_OK) {
    return false;
  }
  rx_head = 0U;
  rx_tail = 0U;
  rx_count = 0U;
  error_events = 0U;
  event_counters = (BspFdcanEventCounters){0};
  return HAL_FDCAN_Start(&BOARD_FDCAN) == HAL_OK;
}

bool BspFdcan_Restart(void)
{
  if (HAL_FDCAN_Stop(&BOARD_FDCAN) != HAL_OK) {
    return false;
  }
  return HAL_FDCAN_Start(&BOARD_FDCAN) == HAL_OK;
}

bool BspFdcan_TakeRxFrame(BspFdcanFrame *frame)
{
  uint32_t primask;

  if (frame == NULL) {
    return false;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (rx_count == 0U) {
    __set_PRIMASK(primask);
    return false;
  }
  *frame = rx_queue[rx_tail];
  rx_tail = (uint8_t)((rx_tail + 1U) % BSP_FDCAN_RX_QUEUE_DEPTH);
  --rx_count;
  __set_PRIMASK(primask);
  return true;
}

static bool ReadRxFrameFromIsr(BspFdcanFrame *frame)
{
  FDCAN_RxHeaderTypeDef header = {0};

  if (frame == NULL ||
      HAL_FDCAN_GetRxMessage(&BOARD_FDCAN, FDCAN_RX_FIFO0, &header,
                             frame->data) != HAL_OK ||
      header.FDFormat != FDCAN_FD_CAN ||
      header.BitRateSwitch != FDCAN_BRS_ON) {
    return false;
  }

  frame->identifier = header.Identifier;
  frame->length = DlcToLength(header.DataLength);
  if (frame->length == 0U && header.DataLength != FDCAN_DLC_BYTES_0) {
    return false;
  }
  return true;
}

bool BspFdcan_SendFrame(const BspFdcanFrame *frame)
{
  return SendFrame(frame, false, 0U);
}

bool BspFdcan_SendFrameWithTxEvent(
    const BspFdcanFrame *frame, uint8_t message_marker)
{
  return SendFrame(frame, true, message_marker);
}

bool BspFdcan_TakeTxEvent(uint8_t *message_marker)
{
  FDCAN_TxEventFifoTypeDef event = {0};

  if (message_marker == NULL ||
      (BOARD_FDCAN.Instance->TXEFS & FDCAN_TXEFS_EFFL) == 0U ||
      HAL_FDCAN_GetTxEvent(&BOARD_FDCAN, &event) != HAL_OK ||
      event.EventType != FDCAN_TX_EVENT) {
    return false;
  }
  *message_marker = (uint8_t)event.MessageMarker;
  return true;
}

static bool SendFrame(const BspFdcanFrame *frame, bool store_tx_event,
                      uint8_t message_marker)
{
  FDCAN_TxHeaderTypeDef header = {0};
  uint8_t data[BSP_FDCAN_MAX_DATA_SIZE] = {0};
  uint32_t dlc;

  if (frame == NULL || frame->identifier > 0x7FFU ||
      !LengthToDlc(frame->length, &dlc)) {
    return false;
  }

  memcpy(data, frame->data, frame->length);
  header.Identifier = frame->identifier;
  header.IdType = FDCAN_STANDARD_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = dlc;
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = FDCAN_BRS_ON;
  header.FDFormat = FDCAN_FD_CAN;
  header.TxEventFifoControl = store_tx_event
                                  ? FDCAN_STORE_TX_EVENTS
                                  : FDCAN_NO_TX_EVENTS;
  header.MessageMarker = message_marker;
  return HAL_FDCAN_AddMessageToTxFifoQ(&BOARD_FDCAN, &header, data) == HAL_OK;
}

uint32_t BspFdcan_TakeErrorEvents(void)
{
  return __atomic_exchange_n(&error_events, 0U, __ATOMIC_RELAXED);
}

void BspFdcan_GetEventCounters(BspFdcanEventCounters *counters)
{
  if (counters == NULL) {
    return;
  }
  counters->warning_count =
      __atomic_load_n(&event_counters.warning_count, __ATOMIC_RELAXED);
  counters->error_passive_count =
      __atomic_load_n(&event_counters.error_passive_count, __ATOMIC_RELAXED);
  counters->bus_off_count =
      __atomic_load_n(&event_counters.bus_off_count, __ATOMIC_RELAXED);
  counters->protocol_error_count =
      __atomic_load_n(&event_counters.protocol_error_count, __ATOMIC_RELAXED);
  counters->rx_fifo_full_count =
      __atomic_load_n(&event_counters.rx_fifo_full_count, __ATOMIC_RELAXED);
  counters->rx_fifo_lost_count =
      __atomic_load_n(&event_counters.rx_fifo_lost_count, __ATOMIC_RELAXED);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_its)
{
  BspFdcanFrame frame = {0};

  if (hfdcan != &BOARD_FDCAN) {
    return;
  }
  if ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_FULL) != 0U) {
    (void)__atomic_fetch_add(&event_counters.rx_fifo_full_count, 1U,
                            __ATOMIC_RELAXED);
    (void)__atomic_fetch_or(&error_events, BSP_FDCAN_ERROR_RX_FIFO_FULL,
                            __ATOMIC_RELAXED);
  }
  if ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != 0U) {
    (void)__atomic_fetch_add(&event_counters.rx_fifo_lost_count, 1U,
                            __ATOMIC_RELAXED);
    (void)__atomic_fetch_or(&error_events, BSP_FDCAN_ERROR_RX_FIFO_LOST,
                            __ATOMIC_RELAXED);
  }
  if ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U ||
      !ReadRxFrameFromIsr(&frame)) {
    return;
  }
  if (rx_count >= BSP_FDCAN_RX_QUEUE_DEPTH) {
    (void)__atomic_fetch_add(&event_counters.rx_fifo_lost_count, 1U,
                            __ATOMIC_RELAXED);
    (void)__atomic_fetch_or(&error_events, BSP_FDCAN_ERROR_RX_FIFO_LOST,
                            __ATOMIC_RELAXED);
    return;
  }
  rx_queue[rx_head] = frame;
  rx_head = (uint8_t)((rx_head + 1U) % BSP_FDCAN_RX_QUEUE_DEPTH);
  ++rx_count;
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan,
                                   uint32_t error_status_its)
{
  uint32_t events = 0U;

  if (hfdcan != &BOARD_FDCAN) {
    return;
  }
  if ((error_status_its & FDCAN_IT_ERROR_WARNING) != 0U) {
    (void)__atomic_fetch_add(&event_counters.warning_count, 1U,
                            __ATOMIC_RELAXED);
    events |= BSP_FDCAN_ERROR_WARNING;
  }
  if ((error_status_its & FDCAN_IT_ERROR_PASSIVE) != 0U) {
    (void)__atomic_fetch_add(&event_counters.error_passive_count, 1U,
                            __ATOMIC_RELAXED);
    events |= BSP_FDCAN_ERROR_PASSIVE;
  }
  if ((error_status_its & FDCAN_IT_BUS_OFF) != 0U) {
    (void)__atomic_fetch_add(&event_counters.bus_off_count, 1U,
                            __ATOMIC_RELAXED);
    events |= BSP_FDCAN_ERROR_BUS_OFF;
  }
  if ((error_status_its &
       (FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_DATA_PROTOCOL_ERROR)) != 0U) {
    (void)__atomic_fetch_add(&event_counters.protocol_error_count, 1U,
                            __ATOMIC_RELAXED);
    events |= BSP_FDCAN_ERROR_PROTOCOL;
  }
  (void)__atomic_fetch_or(&error_events, events, __ATOMIC_RELAXED);
}

bool BspFdcan_IsTxIdle(void)
{
  return BOARD_FDCAN.Instance->TXBRP == 0U;
}

static bool LengthToDlc(uint8_t length, uint32_t *dlc)
{
  static const uint8_t lengths[] = {
      0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
      8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U,
  };
  uint32_t index;

  if (dlc == NULL) {
    return false;
  }
  for (index = 0U; index < sizeof(lengths); ++index) {
    if (lengths[index] == length) {
      *dlc = index;
      return true;
    }
  }
  return false;
}

static uint8_t DlcToLength(uint32_t dlc)
{
  static const uint8_t lengths[] = {
      0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
      8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U,
  };

  return dlc < sizeof(lengths) ? lengths[dlc] : 0U;
}

bool BspFdcan_GetDiagnostics(BspFdcanDiagnostics *diagnostics)
{
  FDCAN_ProtocolStatusTypeDef protocol_status = {0};
  FDCAN_ErrorCountersTypeDef error_counters = {0};

  if (diagnostics == NULL ||
      HAL_FDCAN_GetProtocolStatus(&BOARD_FDCAN, &protocol_status) != HAL_OK ||
      HAL_FDCAN_GetErrorCounters(&BOARD_FDCAN, &error_counters) != HAL_OK) {
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
      HAL_FDCAN_IsRestrictedOperationMode(&BOARD_FDCAN);
  diagnostics->rx_fifo_fill =
      HAL_FDCAN_GetRxFifoFillLevel(&BOARD_FDCAN, FDCAN_RX_FIFO0);
  diagnostics->tx_fifo_free = HAL_FDCAN_GetTxFifoFreeLevel(&BOARD_FDCAN);
  return true;
}
