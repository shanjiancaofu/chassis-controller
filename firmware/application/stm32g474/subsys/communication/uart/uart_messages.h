#ifndef UART_MESSAGES_H
#define UART_MESSAGES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  UART_MESSAGES_LOG_DEBUG = 0,
  UART_MESSAGES_LOG_INFO,
  UART_MESSAGES_LOG_WARN,
  UART_MESSAGES_LOG_ERROR
} UartMessagesLogLevel;

typedef struct {
  const char *section;
  const char *fields;
} UartMessagesTelemetryLine;

void UartMessages_Init(void);
uint32_t UartMessages_NextTelemetrySequence(void);
bool UartMessages_SendResponse(uint32_t now_ms, const char *command,
                               bool success, const char *fields);
bool UartMessages_SendLog(uint32_t now_ms, UartMessagesLogLevel level,
                          const char *module, const char *event,
                          const char *fields);
bool UartMessages_SendTelemetry(uint32_t now_ms, uint32_t sequence,
                                const char *section, const char *fields);
bool UartMessages_SendTelemetryBlock(
    uint32_t now_ms, uint32_t sequence,
    const UartMessagesTelemetryLine *lines, size_t line_count);
bool UartMessages_FormatSigned64(char *buffer, size_t capacity,
                                 int64_t value);

#endif
