#include "bsp/uart/uart_bsp.h"

#include <string.h>

#include "usart.h"

#define UART_DMA_RX_BUFFER_SIZE 128U
#define UART_RX_RING_SIZE 256U
#define UART_TX_QUEUE_DEPTH 8U
#define UART_TX_MESSAGE_SIZE 1200U

typedef struct {
  uint16_t length;
  uint8_t data[UART_TX_MESSAGE_SIZE];
} UartTxMessage;

static uint8_t dma_rx_buffer[UART_DMA_RX_BUFFER_SIZE];
static uint16_t dma_rx_position;
static uint8_t rx_ring[UART_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint32_t rx_overflow_count;
static volatile uint32_t rx_error_count;
static volatile uint32_t rx_restart_count;
static volatile bool rx_restart_pending;

static UartTxMessage tx_queue[UART_TX_QUEUE_DEPTH];
static volatile uint8_t tx_head;
static uint8_t tx_tail;
static volatile uint8_t tx_count;
static volatile bool tx_active;
static volatile uint32_t tx_queue_full_count;
static volatile uint32_t tx_error_count;

static void StoreReceivedBytes(uint16_t position);
static void StoreReceivedByte(uint8_t value);
static void RestartReceiveIfNeeded(void);
static void StartNextTransmit(void);
static void CompleteActiveTransmit(bool success);

bool BspUart_Start(void)
{
  dma_rx_position = 0U;
  rx_head = 0U;
  rx_tail = 0U;
  rx_overflow_count = 0U;
  rx_error_count = 0U;
  rx_restart_count = 0U;
  rx_restart_pending = false;
  tx_head = 0U;
  tx_tail = 0U;
  tx_count = 0U;
  tx_active = false;
  tx_queue_full_count = 0U;
  tx_error_count = 0U;

  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buffer,
                                   sizeof(dma_rx_buffer)) != HAL_OK) {
    return false;
  }

  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  return true;
}

void BspUart_Run(void)
{
  uint32_t primask = __get_PRIMASK();

  RestartReceiveIfNeeded();

  __disable_irq();
  StartNextTransmit();
  __set_PRIMASK(primask);
}

size_t BspUart_Read(uint8_t *data, size_t capacity)
{
  size_t count = 0U;

  if (data == NULL) {
    return 0U;
  }

  while (count < capacity && rx_tail != rx_head) {
    data[count++] = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % UART_RX_RING_SIZE);
  }
  return count;
}

bool BspUart_Write(const void *data, size_t length)
{
  uint8_t slot;
  uint32_t primask;

  if (data == NULL || length == 0U || length > UART_TX_MESSAGE_SIZE) {
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (tx_count >= UART_TX_QUEUE_DEPTH) {
    ++tx_queue_full_count;
    __set_PRIMASK(primask);
    return false;
  }
  slot = tx_tail;
  memcpy(tx_queue[slot].data, data, length);
  tx_queue[slot].length = (uint16_t)length;
  tx_tail = (uint8_t)((tx_tail + 1U) % UART_TX_QUEUE_DEPTH);
  ++tx_count;
  StartNextTransmit();
  __set_PRIMASK(primask);
  return true;
}

bool BspUart_WriteString(const char *text)
{
  return text != NULL && BspUart_Write(text, strlen(text));
}

void BspUart_GetDiagnostics(BspUartDiagnostics *diagnostics)
{
  uint16_t head;
  uint16_t tail;
  uint32_t primask;

  if (diagnostics == NULL) {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  head = rx_head;
  tail = rx_tail;
  diagnostics->rx_overflow_count = rx_overflow_count;
  diagnostics->rx_error_count = rx_error_count;
  diagnostics->rx_restart_count = rx_restart_count;
  diagnostics->tx_queue_full_count = tx_queue_full_count;
  diagnostics->tx_error_count = tx_error_count;
  diagnostics->tx_messages_pending = tx_count;
  diagnostics->tx_active = tx_active;
  __set_PRIMASK(primask);

  diagnostics->rx_bytes_available =
      head >= tail ? (uint16_t)(head - tail)
                   : (uint16_t)(UART_RX_RING_SIZE - tail + head);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart == &huart1 && size <= UART_DMA_RX_BUFFER_SIZE) {
    StoreReceivedBytes(size);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1) {
    CompleteActiveTransmit(true);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1) {
    if (huart->RxState == HAL_UART_STATE_READY) {
      ++rx_error_count;
      rx_restart_pending = true;
    }
    if (tx_active && huart->gState == HAL_UART_STATE_READY) {
      CompleteActiveTransmit(false);
    }
  }
}

static void StoreReceivedBytes(uint16_t position)
{
  uint16_t index;

  if (position > dma_rx_position) {
    for (index = dma_rx_position; index < position; ++index) {
      StoreReceivedByte(dma_rx_buffer[index]);
    }
  } else if (position < dma_rx_position) {
    for (index = dma_rx_position; index < UART_DMA_RX_BUFFER_SIZE; ++index) {
      StoreReceivedByte(dma_rx_buffer[index]);
    }
    for (index = 0U; index < position; ++index) {
      StoreReceivedByte(dma_rx_buffer[index]);
    }
  }

  dma_rx_position = position == UART_DMA_RX_BUFFER_SIZE ? 0U : position;
}

static void StoreReceivedByte(uint8_t value)
{
  const uint16_t next = (uint16_t)((rx_head + 1U) % UART_RX_RING_SIZE);

  if (next == rx_tail) {
    ++rx_overflow_count;
    return;
  }
  rx_ring[rx_head] = value;
  rx_head = next;
}

static void RestartReceiveIfNeeded(void)
{
  HAL_StatusTypeDef status;

  if (!rx_restart_pending || huart1.RxState != HAL_UART_STATE_READY) {
    return;
  }

  rx_restart_pending = false;
  dma_rx_position = 0U;
  status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buffer,
                                       sizeof(dma_rx_buffer));
  if (status == HAL_OK) {
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    ++rx_restart_count;
  } else {
    rx_restart_pending = true;
  }
}

static void StartNextTransmit(void)
{
  if (tx_active || tx_count == 0U || huart1.gState != HAL_UART_STATE_READY) {
    return;
  }

  if (HAL_UART_Transmit_DMA(&huart1, tx_queue[tx_head].data,
                            tx_queue[tx_head].length) == HAL_OK) {
    tx_active = true;
  }
}

static void CompleteActiveTransmit(bool success)
{
  if (!tx_active || tx_count == 0U) {
    return;
  }

  if (!success) {
    ++tx_error_count;
  }
  tx_head = (uint8_t)((tx_head + 1U) % UART_TX_QUEUE_DEPTH);
  --tx_count;
  tx_active = false;
  StartNextTransmit();
}
