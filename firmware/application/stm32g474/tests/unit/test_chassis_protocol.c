#ifdef CHASSIS_PROTOCOL_HOST_TEST

#include <assert.h>
#include <string.h>

#include "subsys/communication/can/chassis_protocol.h"
#include "subsys/communication/can/chassis_protocol_ids.h"

static struct can_frame sent_frame;
static uint32_t send_count;
static int send_result;

static int Send(const struct can_frame *frame)
{
  sent_frame = *frame;
  ++send_count;
  return send_result;
}

static struct can_frame WheelFrame(uint8_t sequence, int16_t left,
                                   int16_t right)
{
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

int main(void)
{
  static const uint8_t ping[8] = {'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U};
  static const uint8_t pass[8] = {'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U};
  const ChassisProtocolPort port = {.send = Send};
  ChassisProtocolWheelRawCommand wheel;
  ChassisProtocolControlCommand control;
  ChassisProtocolPeerHeartbeatSnapshot peer;
  ChassisProtocolVelocityCommand velocity = {
      .enabled = true,
      .sequence = 7U,
      .linear_velocity_mm_s = 500,
      .angular_velocity_mrad_s = -250,
  };
  ChassisProtocolHeartbeat heartbeat = {
      .node_state = CHASSIS_PROTOCOL_NODE_RUNNING,
      .sequence = 40U,
      .flags = CHASSIS_PROTOCOL_HEARTBEAT_FLAG_SENDER_READY,
      .uptime_ms = 123456U,
      .fault_summary = 0x0030U,
  };
  struct can_frame frame;

  assert(ChassisProtocol_Init(&port));
  send_result = 0;

  frame = WheelFrame(1U, 10, -10);
  ChassisProtocol_ProcessFrame(&frame, 1U);
  assert(!ChassisProtocol_TakeControlCommand(&control));

  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  assert(frame.id == CHASSIS_CAN_ID_CMD_VELOCITY && frame.dlc == 12U);
  assert(frame.data[4] == 0xF4U && frame.data[5] == 0x01U);
  assert(frame.data[6] == 0x06U && frame.data[7] == 0xFFU);
  assert(frame.data[10] == 0x0DU && frame.data[11] == 0xC9U);
  ChassisProtocol_ProcessFrame(&frame, 2U);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(control.mode == CHASSIS_PROTOCOL_CONTROL_VELOCITY);
  assert(control.enabled && control.sequence == 7U);
  assert(control.target.velocity.linear_velocity_mm_s == 500);
  assert(control.target.velocity.angular_velocity_mrad_s == -250);

  frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_DEV_HANDSHAKE_REQUEST,
      .dlc = 8U,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  memcpy(frame.data, ping, sizeof(ping));
  ChassisProtocol_ProcessFrame(&frame, 3U);
  assert(!ChassisProtocol_TakeTransportInvalidated());
  assert(ChassisProtocol_GetDevelopmentHandshakeStatus() ==
         CHASSIS_PROTOCOL_DEV_HANDSHAKE_READY);
  ChassisProtocol_Run(3U);
  assert(send_count == 1U);
  assert(sent_frame.id == CHASSIS_CAN_ID_DEV_HANDSHAKE_RESPONSE);
  assert(memcmp(sent_frame.data, "CHASSIS\x01", 8U) == 0);

  frame = WheelFrame(8U, 10, -10);
  ChassisProtocol_ProcessFrame(&frame, 4U);
  assert(!ChassisProtocol_TakeControlCommand(&control));

  frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_DEV_HANDSHAKE_REQUEST,
      .dlc = 8U,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  memcpy(frame.data, pass, sizeof(pass));
  ChassisProtocol_ProcessFrame(&frame, 5U);
  assert(ChassisProtocol_GetDevelopmentHandshakeStatus() ==
         CHASSIS_PROTOCOL_DEV_HANDSHAKE_PASSED);
  frame = WheelFrame(8U, 10, -10);
  ChassisProtocol_ProcessFrame(&frame, 6U);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(control.mode == CHASSIS_PROTOCOL_CONTROL_WHEEL_RAW);
  assert(control.sequence == 8U);
  assert(control.target.wheel_raw.left_target == 10);
  assert(control.target.wheel_raw.right_target == -10);
  ChassisProtocol_ProcessFrame(&frame, 7U);
  assert(!ChassisProtocol_TakeControlCommand(&control));

  frame = WheelFrame(9U, 101, 0);
  assert(ChassisProtocol_DecodeWheelRaw(&frame, &wheel) ==
         CHASSIS_PROTOCOL_DECODE_RANGE);
  frame.dlc = 7U;
  assert(ChassisProtocol_DecodeWheelRaw(&frame, &wheel) ==
         CHASSIS_PROTOCOL_DECODE_WRONG_LENGTH);

  velocity.angular_velocity_mrad_s = 10001;
  assert(!ChassisProtocol_EncodeVelocity(&velocity, &frame));
  velocity.enabled = false;
  velocity.linear_velocity_mm_s = 1;
  velocity.angular_velocity_mrad_s = 0;
  assert(!ChassisProtocol_EncodeVelocity(&velocity, &frame));

  assert(ChassisProtocol_EncodeHeartbeat(&heartbeat, &frame));
  assert(ChassisProtocol_DecodeHeartbeat(&frame, &heartbeat) ==
         CHASSIS_PROTOCOL_DECODE_OK);
  ChassisProtocol_ProcessFrame(&frame, 100U);
  ChassisProtocol_GetPeerHeartbeat(100U, &peer);
  assert(peer.status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_ALIVE);
  assert(peer.heartbeat.sequence == 40U && peer.age_ms == 0U);
  ChassisProtocol_Run(399U);
  ChassisProtocol_GetPeerHeartbeat(399U, &peer);
  assert(peer.status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_ALIVE);
  assert(peer.age_ms == 299U);
  ChassisProtocol_Run(400U);
  ChassisProtocol_GetPeerHeartbeat(400U, &peer);
  assert(peer.status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_TIMEOUT);
  assert(!ChassisProtocol_TakeControlCommand(&control));

  ChassisProtocol_ProcessFrame(&frame, 450U);
  ChassisProtocol_GetPeerHeartbeat(450U, &peer);
  assert(peer.status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_TIMEOUT);
  heartbeat.sequence = 0U;
  assert(ChassisProtocol_EncodeHeartbeat(&heartbeat, &frame));
  ChassisProtocol_ProcessFrame(&frame, 500U);
  ChassisProtocol_GetPeerHeartbeat(500U, &peer);
  assert(peer.status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_ALIVE);
  assert(peer.heartbeat.sequence == 0U);
  frame.data[4] ^= 1U;
  ChassisProtocol_ProcessFrame(&frame, 550U);
  ChassisProtocol_GetPeerHeartbeat(550U, &peer);
  assert(peer.received_ms == 500U && peer.age_ms == 50U);

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
    assert(((uint16_t)frame.data[14] |
            ((uint16_t)frame.data[15] << 8U)) ==
           ChassisProtocol_Crc16(frame.id, frame.data, 14U));
  }

  ChassisProtocol_InvalidateTransport();
  assert(ChassisProtocol_TakeTransportInvalidated());
  ChassisProtocol_GetPeerHeartbeat(600U, &peer);
  assert(peer.status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_UNKNOWN);
  return 0;
}

#endif
