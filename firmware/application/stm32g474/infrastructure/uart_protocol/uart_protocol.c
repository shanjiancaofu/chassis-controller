#include "infrastructure/uart_protocol/uart_protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsp/uart/uart_bsp.h"

static uint32_t next_telemetry_sequence;
static char message_buffer[BSP_UART_MAX_WRITE_SIZE];

bool UartProtocol_FormatSigned64(char *buffer, size_t capacity,
                                int64_t value)
{
  char reversed[20];
  uint64_t magnitude;
  size_t digit_count = 0U;
  size_t output_index = 0U;

  if (buffer == NULL || capacity < 2U) {
    return false;
  }
  if (value < 0) {
    magnitude = (uint64_t)(-(value + 1)) + 1U;
    buffer[output_index++] = '-';
  } else {
    magnitude = (uint64_t)value;
  }
  do {
    reversed[digit_count++] = (char)('0' + (magnitude % 10U));
    magnitude /= 10U;
  } while (magnitude != 0U);

  if (output_index + digit_count + 1U > capacity) {
    return false;
  }
  while (digit_count > 0U) {
    buffer[output_index++] = reversed[--digit_count];
  }
  buffer[output_index] = '\0';
  return true;
}

static bool SendFormattedMessage(const char *format, ...)
{
  va_list arguments;
  int length;

  va_start(arguments, format);
  length = vsnprintf(message_buffer, sizeof(message_buffer), format, arguments);
  va_end(arguments);

  return length > 0 && (size_t)length < sizeof(message_buffer) &&
         BspUart_Write(message_buffer, (size_t)length);
}

static const char *LogLevelText(UartProtocolLogLevel level)
{
  switch (level) {
    case UART_PROTOCOL_LOG_DEBUG:
      return "DEBUG";
    case UART_PROTOCOL_LOG_INFO:
      return "INFO";
    case UART_PROTOCOL_LOG_WARN:
      return "WARN";
    case UART_PROTOCOL_LOG_ERROR:
      return "ERROR";
    default:
      return "ERROR";
  }
}

static const char *OptionalFieldsSeparator(const char *fields)
{
  return fields != NULL && fields[0] != '\0' ? " " : "";
}

static const char *OptionalFieldsText(const char *fields)
{
  return fields != NULL ? fields : "";
}

static bool FormatTelemetryMessage(uint32_t now_ms, uint32_t sequence,
                                   const UartProtocolTelemetryLine *line,
                                   size_t *length)
{
  int formatted_length;

  if (line == NULL || line->section == NULL || line->section[0] == '\0' ||
      length == NULL) {
    return false;
  }
  formatted_length = snprintf(
      message_buffer, sizeof(message_buffer),
      "[TEL] v=1 ts_ms=%lu seq=%lu section=%s%s%s\r\n",
      (unsigned long)now_ms, (unsigned long)sequence, line->section,
      OptionalFieldsSeparator(line->fields),
      OptionalFieldsText(line->fields));
  if (formatted_length <= 0 ||
      (size_t)formatted_length >= sizeof(message_buffer)) {
    return false;
  }
  *length = (size_t)formatted_length;
  return true;
}

void UartProtocol_Init(void)
{
  next_telemetry_sequence = 0U;
  (void)memset(message_buffer, 0, sizeof(message_buffer));
}

uint32_t UartProtocol_NextTelemetrySequence(void)
{
  const uint32_t sequence = next_telemetry_sequence;

  ++next_telemetry_sequence;
  return sequence;
}

bool UartProtocol_SendResponse(uint32_t now_ms, const char *command,
                               bool success, const char *fields)
{
  if (command == NULL || command[0] == '\0') {
    return false;
  }

  return SendFormattedMessage(
      "[RSP] v=1 ts_ms=%lu result=%s command=%s%s%s\r\n",
      (unsigned long)now_ms, success ? "OK" : "ERROR", command,
      OptionalFieldsSeparator(fields), OptionalFieldsText(fields));
}

bool UartProtocol_SendLog(uint32_t now_ms, UartProtocolLogLevel level,
                          const char *module, const char *event,
                          const char *fields)
{
  if (module == NULL || module[0] == '\0' || event == NULL ||
      event[0] == '\0') {
    return false;
  }

  return SendFormattedMessage(
      "[LOG] v=1 ts_ms=%lu level=%s module=%s event=%s%s%s\r\n",
      (unsigned long)now_ms, LogLevelText(level), module, event,
      OptionalFieldsSeparator(fields), OptionalFieldsText(fields));
}

bool UartProtocol_SendTelemetry(uint32_t now_ms, uint32_t sequence,
                                const char *section, const char *fields)
{
  const UartProtocolTelemetryLine line = {.section = section,
                                          .fields = fields};

  return UartProtocol_SendTelemetryBlock(now_ms, sequence, &line, 1U);
}

bool UartProtocol_SendTelemetryBlock(
    uint32_t now_ms, uint32_t sequence,
    const UartProtocolTelemetryLine *lines, size_t line_count)
{
  size_t length;
  size_t index;

  if (lines == NULL || line_count == 0U ||
      line_count > BspUart_GetTxSlotsAvailable()) {
    return false;
  }

  for (index = 0U; index < line_count; ++index) {
    if (!FormatTelemetryMessage(now_ms, sequence, &lines[index], &length)) {
      return false;
    }
  }

  for (index = 0U; index < line_count; ++index) {
    if (!FormatTelemetryMessage(now_ms, sequence, &lines[index], &length) ||
        !BspUart_Write(message_buffer, length)) {
      return false;
    }
  }
  return true;
}
