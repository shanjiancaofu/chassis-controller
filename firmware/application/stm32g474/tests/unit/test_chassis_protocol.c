#ifdef CHASSIS_PROTOCOL_HOST_TEST

#include <assert.h>
#include <string.h>

#include "subsys/communication/chassis_protocol/chassis_protocol.h"
#include "subsys/communication/chassis_protocol/chassis_protocol_ids.h"

static struct can_frame sent_frame;
static uint32_t send_count;

static int Send(const struct can_frame *frame) {
  sent_frame = *frame;
  ++send_count;
  return 0;
}

static struct can_frame WheelFrame(uint8_t sequence, int16_t left,
                                   int16_t right) {
  struct can_frame frame = {
      .id = CHASSIS_CAN_ID_CMD_WHEEL_RAW,
      .dlc = CHASSIS_PROTOCOL_WHEEL_RAW_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
      .data = {1U, 1U, sequence, 0U},
  };
  frame.data[4] = (uint8_t)((uint16_t)left & 0xFFU);
  frame.data[5] = (uint8_t)((uint16_t)left >> 8U);
  frame.data[6] = (uint8_t)((uint16_t)right & 0xFFU);
  frame.data[7] = (uint8_t)((uint16_t)right >> 8U);
  return frame;
}

int main(void) {
  static const uint8_t ping[8] = {'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U};
  static const uint8_t pass[8] = {'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U};
  const ChassisProtocolPort port = {.send = Send};
  ChassisProtocolWheelRawCommand wheel;
  ChassisProtocolVelocityCommand velocity = {
      .enabled = true,
      .sequence = 7U,
      .linear_velocity_mm_s = 500,
      .angular_velocity_mrad_s = -250,
  };
  struct can_frame frame = {
      .id = CHASSIS_CAN_ID_DEV_HANDSHAKE_REQUEST,
      .dlc = 8U,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };

  assert(ChassisProtocol_Init(&port));
  memcpy(frame.data, ping, sizeof(ping));
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_TakeSessionInvalidated());
  ChassisProtocol_Run(0U);
  assert(send_count == 1U);
  assert(sent_frame.id == CHASSIS_CAN_ID_DEV_HANDSHAKE_RESPONSE);
  assert(memcmp(sent_frame.data, "CHASSIS\x01", 8U) == 0);

  memcpy(frame.data, pass, sizeof(pass));
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_GetLinkStatus() == CHASSIS_PROTOCOL_LINK_PASSED);
  frame = WheelFrame(1U, 10, -10);
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_TakeWheelRawCommand(&wheel));
  assert(wheel.enabled && wheel.sequence == 1U);
  assert(wheel.left_target == 10 && wheel.right_target == -10);
  ChassisProtocol_ProcessFrame(&frame);
  assert(!ChassisProtocol_TakeWheelRawCommand(&wheel));
  frame = WheelFrame(2U, 101, 0);
  assert(ChassisProtocol_DecodeWheelRaw(&frame, &wheel) ==
         CHASSIS_PROTOCOL_DECODE_RANGE);
  frame.dlc = 7U;
  assert(ChassisProtocol_DecodeWheelRaw(&frame, &wheel) ==
         CHASSIS_PROTOCOL_DECODE_WRONG_LENGTH);

  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  assert(frame.id == CHASSIS_CAN_ID_CMD_VELOCITY && frame.dlc == 12U);
  assert(frame.data[4] == 0xF4U && frame.data[5] == 0x01U);
  assert(frame.data[6] == 0x06U && frame.data[7] == 0xFFU);
  assert(frame.data[10] == 0x0DU && frame.data[11] == 0xC9U);
  assert(ChassisProtocol_DecodeVelocity(&frame, &velocity) ==
         CHASSIS_PROTOCOL_DECODE_OK);
  frame.data[4] ^= 1U;
  assert(ChassisProtocol_DecodeVelocity(&frame, &velocity) ==
         CHASSIS_PROTOCOL_DECODE_CRC);
  return 0;
}

#endif
