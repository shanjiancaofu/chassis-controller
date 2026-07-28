#include "communication/ota_transport/ota_can_transport.h"

#include <stddef.h>
#include <string.h>

static volatile bool message_pending;
static OtaMessage pending_message;
static volatile uint32_t dropped_count;

static uint32_t ReadU32(const uint8_t *data);
static void WriteU32(uint8_t *data, uint32_t value);

void OtaCanTransport_Init(void)
{
  message_pending = false;
  dropped_count = 0U;
  pending_message = (OtaMessage){0};
}

bool OtaCanTransport_OnRxFrameFromIsr(const BspFdcanFrame *frame)
{
  OtaMessage message = {0};
  uint8_t index;

  if (frame == NULL || frame->identifier != OTA_CAN_REQUEST_ID ||
      frame->length != OTA_CAN_FRAME_SIZE ||
      frame->data[0] != OTA_PROTOCOL_VERSION ||
      frame->data[1] < OTA_MESSAGE_BEGIN ||
      frame->data[1] > OTA_MESSAGE_STATUS ||
      frame->data[3] > OTA_CAN_DATA_SIZE) {
    return false;
  }
  for (index = (uint8_t)(OTA_CAN_HEADER_SIZE + frame->data[3]);
       index < OTA_CAN_FRAME_SIZE; ++index) {
    if (frame->data[index] != 0U) {
      return false;
    }
  }

  if (message_pending) {
    ++dropped_count;
    return false;
  }
  message.source = OTA_SOURCE_CAN_FD;
  message.type = (OtaMessageType)frame->data[1];
  message.session_id = frame->data[2];
  message.data_length = frame->data[3];
  message.argument = ReadU32(&frame->data[4]);
  memcpy(message.data, &frame->data[OTA_CAN_HEADER_SIZE],
         message.data_length);
  pending_message = message;
  message_pending = true;
  return true;
}

bool OtaCanTransport_TakeMessage(OtaMessage *message)
{
  uint32_t primask;

  if (message == NULL) {
    return false;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (!message_pending) {
    __set_PRIMASK(primask);
    return false;
  }
  *message = pending_message;
  message_pending = false;
  __set_PRIMASK(primask);
  return true;
}

bool OtaCanTransport_SendResponse(const OtaResponse *response)
{
  BspFdcanFrame frame = {
      .identifier = OTA_CAN_RESPONSE_ID,
      .length = OTA_CAN_FRAME_SIZE,
  };

  if (response == NULL) {
    return false;
  }
  frame.data[0] = OTA_PROTOCOL_VERSION;
  frame.data[1] = OTA_MESSAGE_STATUS;
  frame.data[2] = response->session_id;
  frame.data[3] = (uint8_t)response->result;
  frame.data[4] = (uint8_t)response->state;
  WriteU32(&frame.data[8], response->next_offset);
  return BspFdcan_SendFrame(&frame) == HAL_OK;
}

bool OtaCanTransport_IsTxIdle(void)
{
  return BspFdcan_IsTxIdle();
}

void OtaCanTransport_Invalidate(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  message_pending = false;
  __set_PRIMASK(primask);
}

uint32_t OtaCanTransport_GetDroppedCount(void)
{
  return __atomic_load_n(&dropped_count, __ATOMIC_RELAXED);
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
