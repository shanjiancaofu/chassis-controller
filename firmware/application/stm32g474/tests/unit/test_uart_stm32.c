#ifdef UART_STM32_HOST_TEST

#include <assert.h>
#include <string.h>

#include "drivers/uart/uart_stm32_private.h"

static DMA_HandleTypeDef dma;
static UART_HandleTypeDef handle = {
    .Instance = (void *)1,
    .gState = HAL_UART_STATE_READY,
    .RxState = HAL_UART_STATE_READY,
    .hdmarx = &dma,
};
static UartStm32Data data;
static const UartStm32Config config = {.handle = &handle};
static struct device_state state = {.init_res = 0, .initialized = true};
const struct device device_uart0 = {
    .name = "uart0", .config = &config, .data = &data, .state = &state,
};

static uint32_t receive_starts;
static uint32_t transmit_starts;

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *uart,
                                               uint8_t *buffer, uint16_t size)
{
  assert(uart == &handle && buffer == data.dma_rx_buffer);
  assert(size == UART_STM32_DMA_RX_BUFFER_SIZE);
  ++receive_starts;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart,
                                        uint8_t *buffer, uint16_t size)
{
  assert(uart == &handle && buffer != NULL && size > 0U);
  ++transmit_starts;
  return HAL_OK;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart);

int main(void)
{
  const struct uart_driver_api *api;
  UartDiagnostics diagnostics;
  uint8_t received[4] = {0};
  uint32_t token;
  bool completed;
  bool success;

  assert(UartStm32_Init(&device_uart0) == 0);
  assert(receive_starts == 1U);
  api = &uart_stm32_api;
  assert(api->write_tracked(&device_uart0, "abc", 3U, &token));
  assert(transmit_starts == 1U);
  HAL_UART_TxCpltCallback(&handle);
  assert(api->get_tracked_completion(&device_uart0, token, &completed,
                                     &success));
  assert(completed && success && api->is_tx_idle(&device_uart0));

  data.dma_rx_buffer[0] = 1U;
  data.dma_rx_buffer[1] = 2U;
  data.dma_rx_buffer[2] = 3U;
  HAL_UARTEx_RxEventCallback(&handle, 3U);
  assert(api->read(&device_uart0, received, sizeof(received)) == 3U);
  assert(memcmp(received, "\x01\x02\x03", 3U) == 0);

  handle.gState = HAL_UART_STATE_BUSY;
  for (uint32_t index = 0U; index < UART_STM32_TX_QUEUE_DEPTH; ++index) {
    assert(api->write(&device_uart0, "x", 1U));
  }
  assert(!api->write(&device_uart0, "x", 1U));
  api->get_diagnostics(&device_uart0, &diagnostics);
  assert(diagnostics.tx_messages_pending == UART_STM32_TX_QUEUE_DEPTH);
  assert(diagnostics.tx_queue_full_count == 1U);

  handle.gState = HAL_UART_STATE_READY;
  api->run(&device_uart0);
  assert(transmit_starts == 2U);
  handle.RxState = HAL_UART_STATE_READY;
  HAL_UART_ErrorCallback(&handle);
  api->run(&device_uart0);
  api->get_diagnostics(&device_uart0, &diagnostics);
  assert(diagnostics.rx_error_count == 1U);
  assert(diagnostics.rx_restart_count == 1U);
  return 0;
}

#endif
