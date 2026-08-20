#include "communication/ota_transport/ota_can_transport.h"

#include <stddef.h>
#include <string.h>

#include "drivers/time.h"
#include "drivers/can/can_stm32_fdcan.h"

#define OTA_CAN_TX_EVENT_TIMEOUT_MS 100U

static volatile bool message_pending;
static OtaMessage pending_message;
static volatile uint32_t dropped_count;
static bool response_in_progress;
static bool response_confirmation_ready;
static bool last_response_confirmed;
static uint8_t tx_marker;
static uint8_t pending_tx_marker;
static uint32_t tx_started_ms;
static const struct device *can_device;

static uint32_t ReadU32(const uint8_t *data);
static void WriteU32(uint8_t *data, uint32_t value);
static void PollTxEvent(void);

void OtaCanTransport_Init(const struct device *device)
{
  can_device = device;
  message_pending = false;
  dropped_count = 0U;
  response_in_progress = false;
  response_confirmation_ready = false;
  last_response_confirmed = false;
  tx_marker = 0U;
  pending_tx_marker = 0U;
  tx_started_ms = 0U;
  pending_message = (OtaMessage){0};
}

bool OtaCanTransport_OnRxFrame(const struct can_frame *frame)
{
  OtaMessage message = {0};
  uint8_t index;

  if (frame == NULL || frame->id != OTA_CAN_REQUEST_ID ||
      frame->dlc != OTA_CAN_FRAME_SIZE ||
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
  if (message == NULL) {
    return false;
  }
  if (!message_pending) {
    return false;
  }
  *message = pending_message;
  message_pending = false;
  return true;
}

bool OtaCanTransport_SendResponse(const OtaResponse *response)
{
  struct can_frame frame = {
      .id = OTA_CAN_RESPONSE_ID,
      .dlc = OTA_CAN_FRAME_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };

  if (response == NULL) {
    return false;
  }
  PollTxEvent();
  if (response_in_progress) {
    return false;
  }
  if (response_confirmation_ready) {
    return true;
  }
  frame.data[0] = OTA_PROTOCOL_VERSION;
  frame.data[1] = OTA_MESSAGE_STATUS;
  frame.data[2] = response->session_id;
  frame.data[3] = (uint8_t)response->result;
  frame.data[4] = (uint8_t)response->state;
  WriteU32(&frame.data[8], response->next_offset);
  do {
    ++tx_marker;
  } while (tx_marker == 0U);
  if (can_stm32_fdcan_send_with_tx_event(can_device, &frame, tx_marker) < 0) {
    return false;
  }
  last_response_confirmed = false;
  pending_tx_marker = tx_marker;
  tx_started_ms = time_uptime_ms();
  response_in_progress = true;
  return false;
}

void OtaCanTransport_ResponseAccepted(void)
{
  response_confirmation_ready = false;
}

bool OtaCanTransport_IsTxIdle(void)
{
  PollTxEvent();
  return last_response_confirmed && !response_in_progress &&
         can_is_tx_idle(can_device);
}

void OtaCanTransport_Invalidate(void)
{
  message_pending = false;
  response_in_progress = false;
  response_confirmation_ready = false;
  last_response_confirmed = false;
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

static void PollTxEvent(void)
{
  uint8_t marker;

  while (can_stm32_fdcan_take_tx_event(can_device, &marker) == 0) {
    if (response_in_progress && marker == pending_tx_marker) {
      response_in_progress = false;
      response_confirmation_ready = true;
      last_response_confirmed = true;
    }
  }
  if (response_in_progress && can_is_tx_idle(can_device) &&
      time_uptime_ms() - tx_started_ms >= OTA_CAN_TX_EVENT_TIMEOUT_MS) {
    response_in_progress = false;
  }
}
