#ifndef CHASSIS_UART_STM32_H
#define CHASSIS_UART_STM32_H

#include "device.h"
#include "drivers/uart.h"

int UartStm32_Init(const struct device *device);
extern const struct uart_driver_api uart_stm32_api;

struct uart_driver_api {
  int (*start)(const struct device *device);
  void (*run)(const struct device *device);
  size_t (*read)(const struct device *device, uint8_t *data, size_t capacity);
  bool (*write)(const struct device *device, const void *data, size_t length);
  bool (*write_tracked)(const struct device *device, const void *data,
                        size_t length, uint32_t *token);
  bool (*get_tracked_completion)(const struct device *device, uint32_t token,
                                 bool *completed, bool *success);
  bool (*is_tx_idle)(const struct device *device);
  uint8_t (*get_tx_slots_available)(const struct device *device);
  void (*get_diagnostics)(const struct device *device,
                          UartDiagnostics *diagnostics);
};

#endif
