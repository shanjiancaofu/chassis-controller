#ifndef CHASSIS_UART_H
#define CHASSIS_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device.h"

#define UART_MAX_WRITE_SIZE 2048U

typedef struct {
  uint32_t rx_overflow_count;
  uint32_t rx_error_count;
  uint32_t rx_restart_count;
  uint32_t tx_queue_full_count;
  uint32_t tx_error_count;
  uint16_t rx_bytes_available;
  uint8_t tx_messages_pending;
  bool tx_active;
} UartDiagnostics;

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

int uart_start(void);
void uart_run(void);
size_t uart_read(uint8_t *data, size_t capacity);
bool uart_write(const void *data, size_t length);
bool uart_write_tracked(const void *data, size_t length, uint32_t *token);
bool uart_get_tracked_completion(uint32_t token, bool *completed, bool *success);
bool uart_is_tx_idle(void);
uint8_t uart_get_tx_slots_available(void);
void uart_get_diagnostics(UartDiagnostics *diagnostics);

#endif
