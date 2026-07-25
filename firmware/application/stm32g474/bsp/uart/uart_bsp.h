#ifndef UART_BSP_H
#define UART_BSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t rx_overflow_count;
  uint32_t rx_error_count;
  uint32_t rx_restart_count;
  uint32_t tx_queue_full_count;
  uint32_t tx_error_count;
  uint16_t rx_bytes_available;
  uint8_t tx_messages_pending;
  bool tx_active;
} BspUartDiagnostics;

bool BspUart_Start(void);
void BspUart_Run(void);
size_t BspUart_Read(uint8_t *data, size_t capacity);
bool BspUart_Write(const void *data, size_t length);
bool BspUart_WriteString(const char *text);
void BspUart_GetDiagnostics(BspUartDiagnostics *diagnostics);

#endif
