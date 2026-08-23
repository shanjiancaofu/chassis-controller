#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  UART_PROTOCOL_LOG_DEBUG = 0,
  UART_PROTOCOL_LOG_INFO,
  UART_PROTOCOL_LOG_WARN,
  UART_PROTOCOL_LOG_ERROR
} UartProtocolLogLevel;

typedef struct {
  const char *section;
  const char *fields;
} UartProtocolTelemetryLine;

void UartProtocol_Init(void);
uint32_t UartProtocol_NextTelemetrySequence(void);
bool UartProtocol_SendResponse(uint32_t now_ms, const char *command,
                               bool success, const char *fields);
bool UartProtocol_SendLog(uint32_t now_ms, UartProtocolLogLevel level,
                          const char *module, const char *event,
                          const char *fields);
bool UartProtocol_SendTelemetry(uint32_t now_ms, uint32_t sequence,
                                const char *section, const char *fields);
bool UartProtocol_SendTelemetryBlock(
    uint32_t now_ms, uint32_t sequence,
    const UartProtocolTelemetryLine *lines, size_t line_count);
bool UartProtocol_FormatSigned64(char *buffer, size_t capacity,
                                 int64_t value);

#endif
