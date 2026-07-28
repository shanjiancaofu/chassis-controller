#include "communication/ota_transport/ota_uart_transport.h"

#include <stddef.h>
#include <string.h>

#include "bsp/uart/uart_bsp.h"
#include "components/crc/crc32.h"

#define UART_OTA_READ_CHUNK_SIZE 32U

static bool enabled;
static bool message_pending;
static OtaMessage pending_message;
static uint8_t frame_buffer[OTA_UART_MAX_FRAME_SIZE];
static uint16_t frame_length;
static uint16_t expected_length;
static uint32_t error_count;

static void ConsumeByte(uint8_t value);
static void ResetParser(void);
static uint32_t ReadU32(const uint8_t *data);
static void WriteU32(uint8_t *data, uint32_t value);

void OtaUartTransport_Init(void)
{
  enabled = false;
  message_pending = false;
  error_count = 0U;
  ResetParser();
}

void OtaUartTransport_Enable(void)
{
  enabled = true;
  message_pending = false;
  ResetParser();
}

void OtaUartTransport_Disable(void)
{
  enabled = false;
  message_pending = false;
  ResetParser();
}

void OtaUartTransport_Run(void)
{
  uint8_t bytes[UART_OTA_READ_CHUNK_SIZE];
  size_t count;
  size_t index;

  if (!enabled) {
    return;
  }
  do {
    count = BspUart_Read(bytes, sizeof(bytes));
    for (index = 0U; index < count; ++index) {
      ConsumeByte(bytes[index]);
    }
  } while (count == sizeof(bytes));
}

bool OtaUartTransport_IsEnabled(void)
{
  return enabled;
}

bool OtaUartTransport_TakeMessage(OtaMessage *message)
{
  if (message == NULL || !message_pending) {
    return false;
  }
  *message = pending_message;
  message_pending = false;
  return true;
}

bool OtaUartTransport_SendResponse(const OtaResponse *response)
{
  uint8_t frame[OTA_UART_HEADER_SIZE + 2U + OTA_UART_CRC_SIZE] = {0};
  uint32_t crc;

  if (response == NULL || !enabled) {
    return false;
  }
  frame[0] = OTA_UART_MAGIC_0;
  frame[1] = OTA_UART_MAGIC_1;
  frame[2] = OTA_PROTOCOL_VERSION;
  frame[3] = OTA_MESSAGE_STATUS;
  frame[4] = response->session_id;
  frame[5] = 2U;
  WriteU32(&frame[8], response->next_offset);
  frame[12] = (uint8_t)response->result;
  frame[13] = (uint8_t)response->state;
  crc = Crc32_Calculate(frame, sizeof(frame) - OTA_UART_CRC_SIZE);
  WriteU32(&frame[sizeof(frame) - OTA_UART_CRC_SIZE], crc);
  return BspUart_Write(frame, sizeof(frame));
}

bool OtaUartTransport_IsTxIdle(void)
{
  return BspUart_IsTxIdle();
}

uint32_t OtaUartTransport_GetErrorCount(void)
{
  return error_count;
}

static void ConsumeByte(uint8_t value)
{
  uint8_t data_length;
  uint32_t received_crc;
  uint32_t calculated_crc;

  if (frame_length == 0U) {
    if (value == OTA_UART_MAGIC_0) {
      frame_buffer[frame_length++] = value;
    }
    return;
  }
  if (frame_length == 1U) {
    if (value == OTA_UART_MAGIC_1) {
      frame_buffer[frame_length++] = value;
    } else if (value != OTA_UART_MAGIC_0) {
      ResetParser();
    }
    return;
  }

  frame_buffer[frame_length++] = value;
  if (frame_length == OTA_UART_HEADER_SIZE) {
    data_length = frame_buffer[5];
    if (frame_buffer[2] != OTA_PROTOCOL_VERSION ||
        frame_buffer[3] < OTA_MESSAGE_BEGIN ||
        frame_buffer[3] > OTA_MESSAGE_STATUS ||
        frame_buffer[6] != 0U || frame_buffer[7] != 0U ||
        data_length > OTA_UART_MAX_DATA_SIZE) {
      ++error_count;
      ResetParser();
      return;
    }
    expected_length = OTA_UART_HEADER_SIZE + data_length +
                      OTA_UART_CRC_SIZE;
  }
  if (expected_length == 0U || frame_length < expected_length) {
    return;
  }

  received_crc = ReadU32(&frame_buffer[expected_length -
                                      OTA_UART_CRC_SIZE]);
  calculated_crc = Crc32_Calculate(
      frame_buffer, expected_length - OTA_UART_CRC_SIZE);
  if (received_crc != calculated_crc || message_pending) {
    ++error_count;
    ResetParser();
    return;
  }

  pending_message = (OtaMessage){0};
  pending_message.source = OTA_SOURCE_UART;
  pending_message.type = (OtaMessageType)frame_buffer[3];
  pending_message.session_id = frame_buffer[4];
  pending_message.data_length = frame_buffer[5];
  pending_message.argument = ReadU32(&frame_buffer[8]);
  memcpy(pending_message.data, &frame_buffer[OTA_UART_HEADER_SIZE],
         pending_message.data_length);
  message_pending = true;
  ResetParser();
}

static void ResetParser(void)
{
  frame_length = 0U;
  expected_length = 0U;
}

static uint32_t ReadU32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void WriteU32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
  data[2] = (uint8_t)(value >> 16U);
  data[3] = (uint8_t)(value >> 24U);
}
