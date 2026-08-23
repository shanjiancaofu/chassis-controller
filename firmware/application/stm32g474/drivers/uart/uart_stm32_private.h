#ifndef CHASSIS_UART_STM32_PRIVATE_H
#define CHASSIS_UART_STM32_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UART_STM32_MAX_WRITE_SIZE 2048U

typedef struct {
  uint32_t rx_overflow_count;
  uint32_t rx_error_count;
  uint32_t rx_restart_count;
  uint32_t tx_queue_full_count;
  uint32_t tx_error_count;
  uint16_t rx_bytes_available;
  uint8_t tx_messages_pending;
  bool tx_active;
} UartStm32Diagnostics;

bool UartStm32Start(void);
void UartStm32Run(void);
size_t UartStm32Read(uint8_t *data, size_t capacity);
bool UartStm32Write(const void *data, size_t length);
bool UartStm32WriteTracked(const void *data, size_t length,
                          uint32_t *token);
bool UartStm32GetTrackedCompletion(uint32_t token, bool *completed,
                                  bool *success);
bool UartStm32WriteString(const char *text);
bool UartStm32IsTxIdle(void);
uint8_t UartStm32GetTxSlotsAvailable(void);
void UartStm32GetDiagnostics(UartStm32Diagnostics *diagnostics);

#endif
