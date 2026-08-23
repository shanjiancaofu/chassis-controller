#include "drivers/uart/uart_stm32_private.h"

#include <errno.h>
#include <string.h>

#include "devicetree.h"

static const UartStm32Config *Config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static UartStm32Data *Data(const struct device *device)
{
  return device != NULL ? (UartStm32Data *)device->data : NULL;
}

static const struct device *UartDeviceFromHandle(UART_HandleTypeDef *handle)
{
  const struct device *device = DEVICE_DT_GET(DT_NODELABEL(uart0));
  const UartStm32Config *config = Config(device);
  return config != NULL && config->handle == handle ? device : NULL;
}

static void StoreReceivedByte(UartStm32Data *data, uint8_t value)
{
  const uint16_t next =
      (uint16_t)((data->rx_head + 1U) % UART_STM32_RX_RING_SIZE);
  if (next == data->rx_tail) {
    ++data->rx_overflow_count;
    return;
  }
  data->rx_ring[data->rx_head] = value;
  data->rx_head = next;
}

static void StoreReceivedBytes(const struct device *device, uint16_t position)
{
  UartStm32Data *data = Data(device);
  if (data == NULL) {
    return;
  }
  if (position > data->dma_rx_position) {
    for (uint16_t index = data->dma_rx_position; index < position; ++index) {
      StoreReceivedByte(data, data->dma_rx_buffer[index]);
    }
  } else if (position < data->dma_rx_position) {
    for (uint16_t index = data->dma_rx_position;
         index < UART_STM32_DMA_RX_BUFFER_SIZE; ++index) {
      StoreReceivedByte(data, data->dma_rx_buffer[index]);
    }
    for (uint16_t index = 0U; index < position; ++index) {
      StoreReceivedByte(data, data->dma_rx_buffer[index]);
    }
  }
  data->dma_rx_position =
      position == UART_STM32_DMA_RX_BUFFER_SIZE ? 0U : position;
}

static void StartNextTransmit(const struct device *device)
{
  const UartStm32Config *config = Config(device);
  UartStm32Data *data = Data(device);
  if (config == NULL || data == NULL || data->tx_active ||
      data->tx_count == 0U ||
      config->handle->gState != HAL_UART_STATE_READY) {
    return;
  }
  if (HAL_UART_Transmit_DMA(config->handle,
                            data->tx_queue[data->tx_head].data,
                            data->tx_queue[data->tx_head].length) == HAL_OK) {
    data->tx_active = true;
  }
}

static void CompleteActiveTransmit(const struct device *device, bool success)
{
  UartStm32Data *data = Data(device);
  if (data == NULL || !data->tx_active || data->tx_count == 0U) {
    return;
  }
  if (!success) {
    ++data->tx_error_count;
  }
  if (data->tx_queue[data->tx_head].tracked) {
    data->completed_tx_success = success;
    data->completed_tx_token = data->tx_queue[data->tx_head].token;
  }
  data->tx_head =
      (uint8_t)((data->tx_head + 1U) % UART_STM32_TX_QUEUE_DEPTH);
  --data->tx_count;
  data->tx_active = false;
  StartNextTransmit(device);
}

static void RestartReceiveIfNeeded(const struct device *device)
{
  const UartStm32Config *config = Config(device);
  UartStm32Data *data = Data(device);
  if (config == NULL || data == NULL || !data->rx_restart_pending ||
      config->handle->RxState != HAL_UART_STATE_READY) {
    return;
  }
  data->rx_restart_pending = false;
  data->dma_rx_position = 0U;
  if (HAL_UARTEx_ReceiveToIdle_DMA(config->handle, data->dma_rx_buffer,
                                   sizeof(data->dma_rx_buffer)) == HAL_OK) {
    __HAL_DMA_DISABLE_IT(config->handle->hdmarx, DMA_IT_HT);
    ++data->rx_restart_count;
  } else {
    data->rx_restart_pending = true;
  }
}

static bool QueueTransmit(const struct device *device, const void *buffer,
                          size_t length, bool tracked, uint32_t *token)
{
  UartStm32Data *data = Data(device);
  uint32_t primask;
  uint8_t slot;
  if (data == NULL || buffer == NULL || length == 0U ||
      length > UART_MAX_WRITE_SIZE || (tracked && token == NULL)) {
    return false;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (data->tx_count >= UART_STM32_TX_QUEUE_DEPTH) {
    ++data->tx_queue_full_count;
    __set_PRIMASK(primask);
    return false;
  }
  slot = data->tx_tail;
  memcpy(data->tx_queue[slot].data, buffer, length);
  data->tx_queue[slot].length = (uint16_t)length;
  data->tx_queue[slot].tracked = tracked;
  if (tracked) {
    if (++data->next_tx_token == 0U) {
      ++data->next_tx_token;
    }
    data->tx_queue[slot].token = data->next_tx_token;
    *token = data->next_tx_token;
  } else {
    data->tx_queue[slot].token = 0U;
  }
  data->tx_tail =
      (uint8_t)((data->tx_tail + 1U) % UART_STM32_TX_QUEUE_DEPTH);
  ++data->tx_count;
  StartNextTransmit(device);
  __set_PRIMASK(primask);
  return true;
}

static int Start(const struct device *device)
{
  const UartStm32Config *config = Config(device);
  UartStm32Data *data = Data(device);
  if (config == NULL || config->handle == NULL || data == NULL) {
    return -EINVAL;
  }
  memset(data, 0, sizeof(*data));
  if (HAL_UARTEx_ReceiveToIdle_DMA(config->handle, data->dma_rx_buffer,
                                   sizeof(data->dma_rx_buffer)) != HAL_OK) {
    return -EIO;
  }
  __HAL_DMA_DISABLE_IT(config->handle->hdmarx, DMA_IT_HT);
  return 0;
}

static void Run(const struct device *device)
{
  uint32_t primask;
  RestartReceiveIfNeeded(device);
  primask = __get_PRIMASK();
  __disable_irq();
  StartNextTransmit(device);
  __set_PRIMASK(primask);
}

static size_t Read(const struct device *device, uint8_t *buffer,
                   size_t capacity)
{
  UartStm32Data *data = Data(device);
  size_t count = 0U;
  if (data == NULL || buffer == NULL) {
    return 0U;
  }
  while (count < capacity && data->rx_tail != data->rx_head) {
    buffer[count++] = data->rx_ring[data->rx_tail];
    data->rx_tail =
        (uint16_t)((data->rx_tail + 1U) % UART_STM32_RX_RING_SIZE);
  }
  return count;
}

static bool Write(const struct device *device, const void *buffer,
                  size_t length)
{
  return QueueTransmit(device, buffer, length, false, NULL);
}

static bool WriteTracked(const struct device *device, const void *buffer,
                         size_t length, uint32_t *token)
{
  return QueueTransmit(device, buffer, length, true, token);
}

static bool GetTrackedCompletion(const struct device *device, uint32_t token,
                                 bool *completed, bool *success)
{
  const UartStm32Data *data = Data(device);
  uint32_t primask;
  if (data == NULL || token == 0U || completed == NULL || success == NULL) {
    return false;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  *completed = data->completed_tx_token == token;
  *success = *completed && data->completed_tx_success;
  __set_PRIMASK(primask);
  return true;
}

static bool IsTxIdle(const struct device *device)
{
  const UartStm32Data *data = Data(device);
  uint32_t primask;
  bool idle;
  if (data == NULL) {
    return false;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  idle = data->tx_count == 0U && !data->tx_active;
  __set_PRIMASK(primask);
  return idle;
}

static uint8_t GetTxSlotsAvailable(const struct device *device)
{
  const UartStm32Data *data = Data(device);
  uint32_t primask;
  uint8_t available;
  if (data == NULL) {
    return 0U;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  available = (uint8_t)(UART_STM32_TX_QUEUE_DEPTH - data->tx_count);
  __set_PRIMASK(primask);
  return available;
}

static void GetDiagnostics(const struct device *device,
                           UartDiagnostics *diagnostics)
{
  const UartStm32Data *data = Data(device);
  uint16_t head;
  uint16_t tail;
  uint32_t primask;
  if (data == NULL || diagnostics == NULL) {
    return;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  head = data->rx_head;
  tail = data->rx_tail;
  diagnostics->rx_overflow_count = data->rx_overflow_count;
  diagnostics->rx_error_count = data->rx_error_count;
  diagnostics->rx_restart_count = data->rx_restart_count;
  diagnostics->tx_queue_full_count = data->tx_queue_full_count;
  diagnostics->tx_error_count = data->tx_error_count;
  diagnostics->tx_messages_pending = data->tx_count;
  diagnostics->tx_active = data->tx_active;
  __set_PRIMASK(primask);
  diagnostics->rx_bytes_available =
      head >= tail ? (uint16_t)(head - tail)
                   : (uint16_t)(UART_STM32_RX_RING_SIZE - tail + head);
}

const struct uart_driver_api uart_stm32_api = {
    .start = Start,
    .run = Run,
    .read = Read,
    .write = Write,
    .write_tracked = WriteTracked,
    .get_tracked_completion = GetTrackedCompletion,
    .is_tx_idle = IsTxIdle,
    .get_tx_slots_available = GetTxSlotsAvailable,
    .get_diagnostics = GetDiagnostics,
};

int UartStm32_Init(const struct device *device)
{
  return Start(device);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *handle, uint16_t size)
{
  const struct device *device = UartDeviceFromHandle(handle);
  if (device != NULL && size <= UART_STM32_DMA_RX_BUFFER_SIZE) {
    StoreReceivedBytes(device, size);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *handle)
{
  const struct device *device = UartDeviceFromHandle(handle);
  if (device != NULL) {
    CompleteActiveTransmit(device, true);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
  const struct device *device = UartDeviceFromHandle(handle);
  UartStm32Data *data = Data(device);
  if (device == NULL || data == NULL) {
    return;
  }
  if (handle->RxState == HAL_UART_STATE_READY) {
    ++data->rx_error_count;
    data->rx_restart_pending = true;
  }
  if (data->tx_active && handle->gState == HAL_UART_STATE_READY) {
    CompleteActiveTransmit(device, false);
  }
}
