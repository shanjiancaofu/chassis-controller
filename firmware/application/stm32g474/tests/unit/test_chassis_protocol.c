#ifdef CHASSIS_PROTOCOL_HOST_TEST

#include <assert.h>
#include <string.h>

#include "subsys/communication/can/chassis_protocol.h"
#include "subsys/communication/can/chassis_protocol_ids.h"

static struct can_frame sent_frame;
static uint32_t send_count;
static int send_result;

static int Send(const struct can_frame *frame) {
  sent_frame = *frame;
  ++send_count;
  return send_result;
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
  ChassisProtocolControlCommand control;
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
  send_result = 0;
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
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(control.mode == CHASSIS_PROTOCOL_CONTROL_WHEEL_RAW);
  assert(control.enabled && control.sequence == 1U);
  assert(control.target.wheel_raw.left_target == 10 &&
         control.target.wheel_raw.right_target == -10);
  ChassisProtocol_ProcessFrame(&frame);
  assert(!ChassisProtocol_TakeControlCommand(&control));
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

  velocity = (ChassisProtocolVelocityCommand){
      .enabled = true,
      .sequence = 2U,
      .linear_velocity_mm_s = 0,
      .angular_velocity_mrad_s = 10001,
  };
  assert(!ChassisProtocol_EncodeVelocity(&velocity, &frame));

  velocity = (ChassisProtocolVelocityCommand){
      .enabled = true,
      .sequence = 2U,
      .linear_velocity_mm_s = 500,
      .angular_velocity_mrad_s = -250,
  };
  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(control.mode == CHASSIS_PROTOCOL_CONTROL_VELOCITY);
  assert(control.sequence == 2U);
  assert(control.target.velocity.linear_velocity_mm_s == 500);
  assert(control.target.velocity.angular_velocity_mrad_s == -250);

  velocity.enabled = false;
  assert(!ChassisProtocol_EncodeVelocity(&velocity, &frame));

  {
    const ChassisProtocolMotionStatus motion = {
        .valid = true,
        .running = true,
        .sequence = 9U,
        .control_state = 1U,
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = -100,
        .linear_velocity_mm_s = 0,
        .angular_velocity_mrad_s = -909,
        .left_output_permille = 250,
        .right_output_permille = -250,
    };
    assert(ChassisProtocol_EncodeMotion(&motion, &frame));
    assert(frame.id == CHASSIS_CAN_ID_STATUS_MOTION && frame.dlc == 16U);
    assert(frame.data[1] == 3U && frame.data[2] == 9U &&
           frame.data[3] == 1U);
    assert(frame.data[4] == 100U && frame.data[5] == 0U);
    assert(frame.data[6] == 0x9CU && frame.data[7] == 0xFFU);
    assert(ChassisProtocol_SendMotion(&motion) == 0);
    assert(sent_frame.data[2] == 0U);
    send_result = -1;
    assert(ChassisProtocol_SendMotion(&motion) == -1);
    assert(sent_frame.data[2] == 1U);
    send_result = 0;
    assert(ChassisProtocol_SendMotion(&motion) == 0);
    assert(sent_frame.data[2] == 1U);
  }
  {
    const ChassisProtocolOdometryReport odometry = {
        .valid = true,
        .sequence = 10U,
        .timestamp_ms = 0x12345678U,
        .x_mm = 1000,
        .y_mm = -1000,
        .heading_mrad = 1571,
        .linear_velocity_mm_s = 500,
        .angular_velocity_mrad_s = -250,
    };
    assert(ChassisProtocol_EncodeOdometry(&odometry, &frame));
    assert(frame.id == CHASSIS_CAN_ID_REPORT_ODOMETRY && frame.dlc == 24U);
    assert(frame.data[1] == 1U && frame.data[2] == 10U);
    assert(frame.data[4] == 0x78U && frame.data[7] == 0x12U);
    assert(frame.data[8] == 0xE8U && frame.data[9] == 0x03U);
  }
  {
    const ChassisProtocolHeartbeat heartbeat = {
        .node_state = CHASSIS_PROTOCOL_NODE_RUNNING,
        .sequence = 11U,
        .flags = CHASSIS_PROTOCOL_HEARTBEAT_FLAG_LINK_PASSED,
        .uptime_ms = 123456U,
        .fault_summary = 0x0030U,
    };
    assert(ChassisProtocol_EncodeHeartbeat(&heartbeat, &frame));
    assert(frame.id == CHASSIS_CAN_ID_HEARTBEAT && frame.dlc == 12U);
    assert(frame.data[1] == CHASSIS_PROTOCOL_NODE_RUNNING &&
           frame.data[2] == 11U);
    assert(((uint16_t)frame.data[10] |
            ((uint16_t)frame.data[11] << 8U)) ==
           ChassisProtocol_Crc16(frame.id, frame.data, 10U));
    assert(ChassisProtocol_SendHeartbeat(&heartbeat) == 0);
    assert(sent_frame.data[2] == 0U);
  }
  {
    const ChassisProtocolFaultStatus fault = {
        .severity = CHASSIS_PROTOCOL_FAULT_CRITICAL,
        .sequence = 12U,
        .flags = CHASSIS_PROTOCOL_FAULT_FLAG_ACTIVE |
                 CHASSIS_PROTOCOL_FAULT_FLAG_CRITICAL,
        .active_faults = 0x00000020U,
        .latched_faults = 0x00000030U,
        .fault_sequence = 5U,
    };
    assert(ChassisProtocol_EncodeFault(&fault, &frame));
    assert(frame.id == CHASSIS_CAN_ID_FAULT_STATUS && frame.dlc == 16U);
    assert(frame.data[1] == CHASSIS_PROTOCOL_FAULT_CRITICAL &&
           frame.data[2] == 12U && frame.data[3] == 3U);
    assert(((uint16_t)frame.data[14] |
            ((uint16_t)frame.data[15] << 8U)) ==
           ChassisProtocol_Crc16(frame.id, frame.data, 14U));
    assert(ChassisProtocol_SendFault(&fault) == 0);
    assert(sent_frame.data[2] == 0U);
  }
  return 0;
}

#endif
