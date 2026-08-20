#include "drivers/uart/uart_stm32.h"

#include "bsp/uart/uart_bsp.h"

static int Start(const struct device *device)
{
  (void)device;
  return BspUart_Start() ? 0 : -1;
}

static void Run(const struct device *device)
{
  (void)device;
  BspUart_Run();
}

static size_t Read(const struct device *device, uint8_t *data, size_t capacity)
{
  (void)device;
  return BspUart_Read(data, capacity);
}

static bool Write(const struct device *device, const void *data, size_t length)
{
  (void)device;
  return BspUart_Write(data, length);
}

static bool WriteTracked(const struct device *device, const void *data,
                         size_t length, uint32_t *token)
{
  (void)device;
  return BspUart_WriteTracked(data, length, token);
}

static bool GetTrackedCompletion(const struct device *device, uint32_t token,
                                 bool *completed, bool *success)
{
  (void)device;
  return BspUart_GetTrackedCompletion(token, completed, success);
}

static bool IsTxIdle(const struct device *device)
{
  (void)device;
  return BspUart_IsTxIdle();
}

static uint8_t GetTxSlotsAvailable(const struct device *device)
{
  (void)device;
  return BspUart_GetTxSlotsAvailable();
}

static void GetDiagnostics(const struct device *device,
                           UartDiagnostics *diagnostics)
{
  BspUartDiagnostics bsp_diagnostics;
  (void)device;
  BspUart_GetDiagnostics(&bsp_diagnostics);
  if (diagnostics == NULL) {
    return;
  }
  *diagnostics = (UartDiagnostics){
      .rx_overflow_count = bsp_diagnostics.rx_overflow_count,
      .rx_error_count = bsp_diagnostics.rx_error_count,
      .rx_restart_count = bsp_diagnostics.rx_restart_count,
      .tx_queue_full_count = bsp_diagnostics.tx_queue_full_count,
      .tx_error_count = bsp_diagnostics.tx_error_count,
      .rx_bytes_available = bsp_diagnostics.rx_bytes_available,
      .tx_messages_pending = bsp_diagnostics.tx_messages_pending,
      .tx_active = bsp_diagnostics.tx_active,
  };
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
  (void)device;
  return Start(device);
}
