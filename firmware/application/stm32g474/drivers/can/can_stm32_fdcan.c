#include "drivers/can/can_stm32_fdcan.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

static const struct device *irq_device;

static const CanStm32FdcanConfig *Config(const struct device *device)
{
  return device->config;
}

static CanStm32FdcanData *Data(const struct device *device)
{
  return device->data;
}

static bool LengthToDlc(uint8_t length, uint32_t *dlc)
{
  static const uint8_t lengths[] = {
      0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
      8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U,
  };

  if (dlc == NULL) {
    return false;
  }
  for (uint32_t index = 0U; index < sizeof(lengths); ++index) {
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

int CanStm32Fdcan_Init(const struct device *device)
{
  const CanStm32FdcanConfig *config;
  CanStm32FdcanData *data;

  if (device == NULL || device->config == NULL || device->data == NULL) {
    return -EINVAL;
  }
  config = Config(device);
  data = Data(device);
  if (config->handle == NULL || config->filter_capacity == 0U ||
      irq_device != NULL) {
    return -EINVAL;
  }
  memset(data, 0, sizeof(*data));
  irq_device = device;
  return 0;
}

static int Start(const struct device *device, const struct can_filter *filters,
                 size_t filter_count)
{
  const CanStm32FdcanConfig *config = Config(device);
  FDCAN_FilterTypeDef filter = {0};

  if (filters == NULL || filter_count == 0U ||
      filter_count > (size_t)config->filter_capacity * 2U) {
    return -EINVAL;
  }
  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  for (size_t index = 0U; index < filter_count; index += 2U) {
    if (filters[index].id > 0x7FFU || filters[index].mask != 0x7FFU ||
        (index + 1U < filter_count &&
         (filters[index + 1U].id > 0x7FFU ||
          filters[index + 1U].mask != 0x7FFU))) {
      return -EINVAL;
    }
    filter.FilterIndex = (uint32_t)(index / 2U);
    filter.FilterID1 = filters[index].id;
    if (index + 1U < filter_count) {
      filter.FilterType = FDCAN_FILTER_DUAL;
      filter.FilterID2 = filters[index + 1U].id;
    } else {
      filter.FilterType = FDCAN_FILTER_MASK;
      filter.FilterID2 = 0x7FFU;
    }
    if (HAL_FDCAN_ConfigFilter(config->handle, &filter) != HAL_OK) {
      return -EIO;
    }
  }
  if (HAL_FDCAN_ConfigGlobalFilter(config->handle, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK ||
      HAL_FDCAN_ActivateNotification(
          config->handle,
          FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL |
              FDCAN_IT_RX_FIFO0_MESSAGE_LOST | FDCAN_IT_ERROR_WARNING |
              FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_BUS_OFF |
              FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_DATA_PROTOCOL_ERROR,
          0U) != HAL_OK || HAL_FDCAN_Start(config->handle) != HAL_OK) {
    return -EIO;
  }
  return 0;
}

static int Stop(const struct device *device)
{
  return HAL_FDCAN_Stop(Config(device)->handle) == HAL_OK ? 0 : -EIO;
}

static int Recover(const struct device *device)
{
  int result = Stop(device);
  if (result < 0) {
    return result;
  }
  return HAL_FDCAN_Start(Config(device)->handle) == HAL_OK ? 0 : -EIO;
}

static int Recv(const struct device *device, struct can_frame *frame)
{
  CanStm32FdcanData *data = Data(device);
  uint32_t primask;

  if (frame == NULL) {
    return -EINVAL;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (data->rx_count == 0U) {
    __set_PRIMASK(primask);
    return -EAGAIN;
  }
  *frame = data->rx_queue[data->rx_tail];
  data->rx_tail = (uint8_t)((data->rx_tail + 1U) % CONFIG_CAN_RX_QUEUE_SIZE);
  --data->rx_count;
  __set_PRIMASK(primask);
  return 0;
}

static int SendInternal(const struct device *device,
                        const struct can_frame *frame, uint8_t token,
                        bool store_tx_event)
{
  FDCAN_HandleTypeDef *handle = Config(device)->handle;
  FDCAN_TxHeaderTypeDef header = {0};
  uint8_t payload[CAN_MAX_DATA_LENGTH] = {0};
  uint32_t dlc;
  HAL_StatusTypeDef status;

  if (frame == NULL || frame->id > 0x7FFU ||
      (frame->flags & (CAN_FRAME_FDF | CAN_FRAME_BRS)) !=
          (CAN_FRAME_FDF | CAN_FRAME_BRS) || !LengthToDlc(frame->dlc, &dlc)) {
    return -EINVAL;
  }
  if (HAL_FDCAN_GetTxFifoFreeLevel(handle) == 0U) {
    return -ENOSPC;
  }
  memcpy(payload, frame->data, frame->dlc);
  header.Identifier = frame->id;
  header.IdType = FDCAN_STANDARD_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = dlc;
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = FDCAN_BRS_ON;
  header.FDFormat = FDCAN_FD_CAN;
  header.TxEventFifoControl = store_tx_event ? FDCAN_STORE_TX_EVENTS
                                             : FDCAN_NO_TX_EVENTS;
  header.MessageMarker = token;
  status = HAL_FDCAN_AddMessageToTxFifoQ(handle, &header, payload);
  return status == HAL_OK ? 0 : status == HAL_BUSY ? -EAGAIN : -EIO;
}

static int Send(const struct device *device, const struct can_frame *frame)
{
  return SendInternal(device, frame, 0U, false);
}

static int GetDiagnostics(const struct device *device, struct can_diagnostics *diagnostics)
{
  const CanStm32FdcanConfig *config = Config(device);
  const CanStm32FdcanData *data = Data(device);
  FDCAN_ProtocolStatusTypeDef protocol = {0};
  FDCAN_ErrorCountersTypeDef errors = {0};

  if (diagnostics == NULL) {
    return -EINVAL;
  }
  if (HAL_FDCAN_GetProtocolStatus(config->handle, &protocol) != HAL_OK ||
      HAL_FDCAN_GetErrorCounters(config->handle, &errors) != HAL_OK) {
    return -EIO;
  }
  *diagnostics = (struct can_diagnostics){
      .last_error_code = protocol.LastErrorCode,
      .data_last_error_code = protocol.DataLastErrorCode,
      .activity = protocol.Activity,
      .tx_error_count = errors.TxErrorCnt,
      .rx_error_count = errors.RxErrorCnt,
      .error_passive = protocol.ErrorPassive,
      .warning = protocol.Warning,
      .bus_off = protocol.BusOff,
      .restricted_mode = HAL_FDCAN_IsRestrictedOperationMode(config->handle),
      .rx_fifo_fill = HAL_FDCAN_GetRxFifoFillLevel(config->handle,
                                                   FDCAN_RX_FIFO0),
      .tx_fifo_free = HAL_FDCAN_GetTxFifoFreeLevel(config->handle),
      .warning_count = data->warning_count,
      .error_passive_count = data->error_passive_count,
      .bus_off_count = data->bus_off_count,
      .protocol_error_count = data->protocol_error_count,
      .rx_fifo_full_count = data->rx_fifo_full_count,
      .rx_fifo_lost_count = data->rx_fifo_lost_count,
  };
  return 0;
}

static uint32_t TakeErrorEvents(const struct device *device)
{
  return __atomic_exchange_n(&Data(device)->error_events, 0U,
                             __ATOMIC_RELAXED);
}

static int TakeTxEvent(const struct device *device, uint8_t *token)
{
  FDCAN_HandleTypeDef *handle = Config(device)->handle;
  FDCAN_TxEventFifoTypeDef event = {0};

  if (token == NULL) {
    return -EINVAL;
  }
  if ((handle->Instance->TXEFS & FDCAN_TXEFS_EFFL) == 0U) {
    return -EAGAIN;
  }
  if (HAL_FDCAN_GetTxEvent(handle, &event) != HAL_OK ||
      event.EventType != FDCAN_TX_EVENT) {
    return -EIO;
  }
  *token = (uint8_t)event.MessageMarker;
  return 0;
}

static bool IsTxIdle(const struct device *device)
{
  return Config(device)->handle->Instance->TXBRP == 0U;
}

const struct can_stm32_fdcan_driver_api can_stm32_fdcan_api = {
    .common = {
        .start = Start,
        .stop = Stop,
        .send = Send,
        .recv = Recv,
        .recover = Recover,
        .get_diagnostics = GetDiagnostics,
        .take_error_events = TakeErrorEvents,
        .is_tx_idle = IsTxIdle,
    },
    .send_with_tx_event = can_stm32_fdcan_send_with_tx_event,
    .take_tx_event = TakeTxEvent,
};

int can_stm32_fdcan_send_with_tx_event(const struct device *device,
                                       const struct can_frame *frame,
                                       uint8_t token)
{
  if (!device_is_ready(device) || device->api != &can_stm32_fdcan_api) {
    return -ENODEV;
  }
  return SendInternal(device, frame, token, true);
}

int can_stm32_fdcan_take_tx_event(const struct device *device, uint8_t *token)
{
  if (!device_is_ready(device) || device->api != &can_stm32_fdcan_api) {
    return -ENODEV;
  }
  return TakeTxEvent(device, token);
}

static bool ReadFrameFromIsr(const struct device *device, struct can_frame *frame)
{
  FDCAN_RxHeaderTypeDef header = {0};

  if (HAL_FDCAN_GetRxMessage(Config(device)->handle, FDCAN_RX_FIFO0, &header,
                             frame->data) != HAL_OK ||
      header.FDFormat != FDCAN_FD_CAN || header.BitRateSwitch != FDCAN_BRS_ON) {
    return false;
  }
  frame->id = header.Identifier;
  frame->dlc = DlcToLength(header.DataLength);
  frame->flags = CAN_FRAME_FDF | CAN_FRAME_BRS;
  return frame->dlc != 0U || header.DataLength == FDCAN_DLC_BYTES_0;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *handle,
                               uint32_t interrupt_flags)
{
  const struct device *device = irq_device;
  CanStm32FdcanData *data;
  struct can_frame frame = {0};

  if (device == NULL || handle != Config(device)->handle) {
    return;
  }
  data = Data(device);
  if ((interrupt_flags & FDCAN_IT_RX_FIFO0_FULL) != 0U) {
    ++data->rx_fifo_full_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_RX_FIFO_FULL,
                            __ATOMIC_RELAXED);
  }
  if ((interrupt_flags & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != 0U) {
    ++data->rx_fifo_lost_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_RX_FIFO_LOST,
                            __ATOMIC_RELAXED);
  }
  if ((interrupt_flags & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U ||
      !ReadFrameFromIsr(device, &frame)) {
    return;
  }
  if (data->rx_count >= CONFIG_CAN_RX_QUEUE_SIZE) {
    ++data->rx_fifo_lost_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_RX_FIFO_LOST,
                            __ATOMIC_RELAXED);
    return;
  }
  data->rx_queue[data->rx_head] = frame;
  data->rx_head = (uint8_t)((data->rx_head + 1U) % CONFIG_CAN_RX_QUEUE_SIZE);
  ++data->rx_count;
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *handle,
                                   uint32_t interrupt_flags)
{
  const struct device *device = irq_device;
  CanStm32FdcanData *data;

  if (device == NULL || handle != Config(device)->handle) {
    return;
  }
  data = Data(device);
  if ((interrupt_flags & FDCAN_IT_ERROR_WARNING) != 0U) {
    ++data->warning_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_WARNING,
                            __ATOMIC_RELAXED);
  }
  if ((interrupt_flags & FDCAN_IT_ERROR_PASSIVE) != 0U) {
    ++data->error_passive_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_PASSIVE,
                            __ATOMIC_RELAXED);
  }
  if ((interrupt_flags & FDCAN_IT_BUS_OFF) != 0U) {
    ++data->bus_off_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_BUS_OFF,
                            __ATOMIC_RELAXED);
  }
  if ((interrupt_flags &
       (FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_DATA_PROTOCOL_ERROR)) != 0U) {
    ++data->protocol_error_count;
    (void)__atomic_fetch_or(&data->error_events, CAN_ERROR_PROTOCOL,
                            __ATOMIC_RELAXED);
  }
}
