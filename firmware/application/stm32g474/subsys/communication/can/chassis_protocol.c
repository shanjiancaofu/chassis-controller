#include "subsys/communication/can/chassis_protocol.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "config/protocol_config.h"
#if !defined(CHASSIS_PROTOCOL_HOST_TEST) && \
    !defined(CHASSIS_CONTROL_FLOW_HOST_TEST)
#include "kernel/critical.h"
#endif
#include "subsys/communication/can/chassis_protocol_ids.h"

#define HANDSHAKE_SIZE 8U
#define RESPONSE_RETRY_LIMIT 3U
#define RESPONSE_RETRY_DELAY_MS 10U

static ChassisProtocolPort protocol_port;
static ChassisProtocolDevelopmentHandshakeStatus development_handshake_status;
static ChassisProtocolPeerHeartbeatStatus peer_heartbeat_status;
static ChassisProtocolHeartbeat peer_heartbeat;
static uint32_t peer_heartbeat_received_ms;
static bool peer_heartbeat_sequence_valid;
static uint8_t last_peer_heartbeat_sequence;
static ChassisProtocolControlCommand pending_control_command;
static bool control_command_pending;
static bool transport_invalidated;
static bool control_sequence_valid;
static uint8_t last_control_sequence;
static uint32_t last_control_received_ms;
static uint8_t motion_tx_sequence;
static uint8_t odometry_tx_sequence;
static uint8_t heartbeat_tx_sequence;
static uint8_t fault_tx_sequence;
static bool response_pending;
static uint32_t response_retry_due_ms;
static uint8_t response_attempts;

static void LockPeerHeartbeat(void) {
#if !defined(CHASSIS_PROTOCOL_HOST_TEST) && \
    !defined(CHASSIS_CONTROL_FLOW_HOST_TEST)
  kernel_critical_enter();
#endif
}

static void UnlockPeerHeartbeat(void) {
#if !defined(CHASSIS_PROTOCOL_HOST_TEST) && \
    !defined(CHASSIS_CONTROL_FLOW_HOST_TEST)
  kernel_critical_exit();
#endif
}

static int16_t GetI16Le(const uint8_t *data) {
  return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static uint16_t GetU16Le(const uint8_t *data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t GetU32Le(const uint8_t *data) {
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void PutI16Le(uint8_t *data, int16_t value) {
  data[0] = (uint8_t)((uint16_t)value & 0xFFU);
  data[1] = (uint8_t)((uint16_t)value >> 8U);
}

static void PutU16Le(uint8_t *data, uint16_t value) {
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)(value >> 8U);
}

static void PutU32Le(uint8_t *data, uint32_t value) {
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)((value >> 8U) & 0xFFU);
  data[2] = (uint8_t)((value >> 16U) & 0xFFU);
  data[3] = (uint8_t)(value >> 24U);
}

static void PutI32Le(uint8_t *data, int32_t value) {
  PutU32Le(data, (uint32_t)value);
}

static void ResetControlSession(void) {
  control_command_pending = false;
  control_sequence_valid = false;
  last_control_sequence = 0U;
  last_control_received_ms = 0U;
  pending_control_command = (ChassisProtocolControlCommand){0};
}

static void ResetPeerHeartbeat(void) {
  LockPeerHeartbeat();
  peer_heartbeat_status = CHASSIS_PROTOCOL_PEER_HEARTBEAT_UNKNOWN;
  peer_heartbeat = (ChassisProtocolHeartbeat){0};
  peer_heartbeat_received_ms = 0U;
  peer_heartbeat_sequence_valid = false;
  last_peer_heartbeat_sequence = 0U;
  UnlockPeerHeartbeat();
}

bool ChassisProtocol_Init(const ChassisProtocolPort *port) {
  if (port == NULL || port->send == NULL) {
    return false;
  }
  protocol_port = *port;
  development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_READY;
  transport_invalidated = false;
  response_pending = false;
  response_retry_due_ms = 0U;
  response_attempts = 0U;
  motion_tx_sequence = 0U;
  odometry_tx_sequence = 0U;
  heartbeat_tx_sequence = 0U;
  fault_tx_sequence = 0U;
  ResetControlSession();
  ResetPeerHeartbeat();
  return true;
}

ChassisProtocolDecodeResult
ChassisProtocol_DecodeWheelRaw(const struct can_frame *frame,
                               ChassisProtocolWheelRawCommand *command) {
  int16_t left;
  int16_t right;

  if (frame == NULL || command == NULL ||
      frame->id != CHASSIS_CAN_ID_CMD_WHEEL_RAW) {
    return CHASSIS_PROTOCOL_DECODE_WRONG_ID;
  }
  if (frame->dlc != CHASSIS_PROTOCOL_WHEEL_RAW_SIZE) {
    return CHASSIS_PROTOCOL_DECODE_WRONG_LENGTH;
  }
  if (frame->data[0] != CHASSIS_PROTOCOL_SCHEMA_VERSION) {
    return CHASSIS_PROTOCOL_DECODE_VERSION;
  }
  if ((frame->data[1] & 0xFEU) != 0U || frame->data[3] != 0U) {
    return CHASSIS_PROTOCOL_DECODE_RESERVED;
  }
  left = GetI16Le(&frame->data[4]);
  right = GetI16Le(&frame->data[6]);
  if (left > CHASSIS_PROTOCOL_WHEEL_RAW_TARGET_LIMIT ||
      left < -CHASSIS_PROTOCOL_WHEEL_RAW_TARGET_LIMIT ||
      right > CHASSIS_PROTOCOL_WHEEL_RAW_TARGET_LIMIT ||
      right < -CHASSIS_PROTOCOL_WHEEL_RAW_TARGET_LIMIT ||
      (frame->data[1] == 0U && (left != 0 || right != 0))) {
    return CHASSIS_PROTOCOL_DECODE_RANGE;
  }
  *command = (ChassisProtocolWheelRawCommand){
      .enabled = frame->data[1] != 0U,
      .sequence = frame->data[2],
      .left_target = left,
      .right_target = right,
  };
  return CHASSIS_PROTOCOL_DECODE_OK;
}

void ChassisProtocol_ProcessFrame(const struct can_frame *frame,
                                  uint32_t now_ms) {
  static const uint8_t request[HANDSHAKE_SIZE] = {'P', 'I', 'N', 'G',
                                                  1U,  0U,  0U,  0U};
  static const uint8_t confirmation[HANDSHAKE_SIZE] = {'P', 'A', 'S', 'S',
                                                       1U,  0U,  0U,  0U};
  ChassisProtocolWheelRawCommand command;
  ChassisProtocolVelocityCommand velocity;
  ChassisProtocolHeartbeat heartbeat;
  ChassisProtocolControlCommand normalized;
  ChassisProtocolDecodeResult decode_result;

  if (frame == NULL) {
    return;
  }
  if (frame->id == CHASSIS_CAN_ID_DEV_HANDSHAKE_REQUEST) {
    if (frame->dlc != HANDSHAKE_SIZE) {
      development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_FAILED;
      response_pending = false;
    } else if (memcmp(frame->data, request, sizeof(request)) == 0) {
      development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_READY;
      response_attempts = 0U;
      response_retry_due_ms = 0U;
      response_pending = true;
    } else if (memcmp(frame->data, confirmation, sizeof(confirmation)) == 0) {
      development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_PASSED;
      response_pending = false;
    } else {
      development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_FAILED;
      response_pending = false;
    }
    return;
  }
  if (frame->id == CHASSIS_CAN_ID_HEARTBEAT) {
    if (ChassisProtocol_DecodeHeartbeat(frame, &heartbeat) ==
        CHASSIS_PROTOCOL_DECODE_OK) {
      uint8_t sequence_delta;

      LockPeerHeartbeat();
      sequence_delta =
          (uint8_t)(heartbeat.sequence - last_peer_heartbeat_sequence);
      if (!peer_heartbeat_sequence_valid ||
          (peer_heartbeat_status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_TIMEOUT &&
           sequence_delta != 0U) ||
          (sequence_delta != 0U && sequence_delta < 128U)) {
        peer_heartbeat = heartbeat;
        peer_heartbeat_received_ms = now_ms;
        last_peer_heartbeat_sequence = heartbeat.sequence;
        peer_heartbeat_sequence_valid = true;
        peer_heartbeat_status = CHASSIS_PROTOCOL_PEER_HEARTBEAT_ALIVE;
      }
      UnlockPeerHeartbeat();
    }
    return;
  }
  if (frame->id == CHASSIS_CAN_ID_CMD_WHEEL_RAW) {
    if (development_handshake_status !=
        CHASSIS_PROTOCOL_DEV_HANDSHAKE_PASSED) {
      return;
    }
    decode_result = ChassisProtocol_DecodeWheelRaw(frame, &command);
    if (decode_result == CHASSIS_PROTOCOL_DECODE_OK) {
      normalized = (ChassisProtocolControlCommand){
          .mode = CHASSIS_PROTOCOL_CONTROL_WHEEL_RAW,
          .enabled = command.enabled,
          .sequence = command.sequence,
          .target.wheel_raw = {.left_target = command.left_target,
                               .right_target = command.right_target},
      };
    }
  } else if (frame->id == CHASSIS_CAN_ID_CMD_VELOCITY) {
    decode_result = ChassisProtocol_DecodeVelocity(frame, &velocity);
    if (decode_result == CHASSIS_PROTOCOL_DECODE_OK) {
      normalized = (ChassisProtocolControlCommand){
          .mode = CHASSIS_PROTOCOL_CONTROL_VELOCITY,
          .enabled = velocity.enabled,
          .sequence = velocity.sequence,
          .target.velocity = {
              .linear_velocity_mm_s = velocity.linear_velocity_mm_s,
              .angular_velocity_mrad_s = velocity.angular_velocity_mrad_s},
      };
    }
  } else {
    return;
  }
  if (decode_result != CHASSIS_PROTOCOL_DECODE_OK) {
    return;
  }
  if (control_sequence_valid &&
      now_ms - last_control_received_ms >= MOTOR_COMMAND_TIMEOUT_MS) {
    control_sequence_valid = false;
  }
  if (control_sequence_valid) {
    const uint8_t sequence_delta =
        (uint8_t)(normalized.sequence - last_control_sequence);

    if (sequence_delta == 0U || sequence_delta >= 128U) {
      return;
    }
  }
  last_control_sequence = normalized.sequence;
  last_control_received_ms = now_ms;
  control_sequence_valid = true;
  pending_control_command = normalized;
  control_command_pending = true;
}

void ChassisProtocol_Run(uint32_t now_ms) {
  static const struct can_frame response = {
      .id = CHASSIS_CAN_ID_DEV_HANDSHAKE_RESPONSE,
      .dlc = HANDSHAKE_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
      .data = {'C', 'H', 'A', 'S', 'S', 'I', 'S', 1U},
  };

  LockPeerHeartbeat();
  if (peer_heartbeat_status == CHASSIS_PROTOCOL_PEER_HEARTBEAT_ALIVE &&
      now_ms - peer_heartbeat_received_ms >=
          CHASSIS_PROTOCOL_HEARTBEAT_TIMEOUT_MS) {
    peer_heartbeat_status = CHASSIS_PROTOCOL_PEER_HEARTBEAT_TIMEOUT;
  }
  UnlockPeerHeartbeat();
  if (control_sequence_valid &&
      now_ms - last_control_received_ms >= MOTOR_COMMAND_TIMEOUT_MS) {
    control_sequence_valid = false;
  }
  if (response_pending &&
      (int32_t)(now_ms - response_retry_due_ms) >= 0) {
    response_pending = false;
    if (protocol_port.send(&response) < 0) {
      ++response_attempts;
      if (response_attempts < RESPONSE_RETRY_LIMIT) {
        response_retry_due_ms = now_ms + RESPONSE_RETRY_DELAY_MS;
        response_pending = true;
      } else {
        response_attempts = 0U;
        development_handshake_status =
            CHASSIS_PROTOCOL_DEV_HANDSHAKE_FAILED;
      }
    } else {
      response_attempts = 0U;
    }
  }
}

void ChassisProtocol_InvalidateTransport(void) {
  development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_FAILED;
  response_pending = false;
  response_attempts = 0U;
  response_retry_due_ms = 0U;
  ResetControlSession();
  ResetPeerHeartbeat();
  transport_invalidated = true;
}

void ChassisProtocol_ResetTransport(void) {
  development_handshake_status = CHASSIS_PROTOCOL_DEV_HANDSHAKE_READY;
  response_pending = false;
  response_attempts = 0U;
  response_retry_due_ms = 0U;
  ResetControlSession();
  ResetPeerHeartbeat();
}

void ChassisProtocol_RequestHandshakeResponse(void) {
  response_attempts = 0U;
  response_retry_due_ms = 0U;
  response_pending = true;
}

ChassisProtocolDevelopmentHandshakeStatus
ChassisProtocol_GetDevelopmentHandshakeStatus(void) {
  return development_handshake_status;
}

void ChassisProtocol_GetPeerHeartbeat(
    uint32_t now_ms, ChassisProtocolPeerHeartbeatSnapshot *snapshot) {
  if (snapshot == NULL) {
    return;
  }
  LockPeerHeartbeat();
  *snapshot = (ChassisProtocolPeerHeartbeatSnapshot){
      .status = peer_heartbeat_status,
      .heartbeat = peer_heartbeat,
      .received_ms = peer_heartbeat_received_ms,
      .age_ms = peer_heartbeat_sequence_valid
                    ? now_ms - peer_heartbeat_received_ms
                    : 0U,
  };
  UnlockPeerHeartbeat();
}

bool ChassisProtocol_TakeTransportInvalidated(void) {
  const bool invalidated = transport_invalidated;
  transport_invalidated = false;
  return invalidated;
}

bool ChassisProtocol_TakeControlCommand(
    ChassisProtocolControlCommand *command) {
  if (command == NULL || !control_command_pending) {
    return false;
  }
  *command = pending_control_command;
  control_command_pending = false;
  return true;
}

uint16_t ChassisProtocol_Crc16(uint16_t identifier, const uint8_t *payload,
                               uint8_t length) {
  uint16_t crc = 0xFFFFU;
  uint8_t header[2] = {(uint8_t)(identifier & 0xFFU),
                       (uint8_t)(identifier >> 8U)};
  for (uint8_t part = 0U; part < 2U; ++part) {
    crc ^= (uint16_t)header[part] << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                  : (uint16_t)(crc << 1U);
    }
  }
  for (uint8_t index = 0U; index < length; ++index) {
    crc ^= (uint16_t)payload[index] << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                  : (uint16_t)(crc << 1U);
    }
  }
  return crc;
}

ChassisProtocolDecodeResult
ChassisProtocol_DecodeVelocity(const struct can_frame *frame,
                               ChassisProtocolVelocityCommand *command) {
  uint16_t expected_crc;
  uint16_t received_crc;
  int16_t linear;

  if (frame == NULL || command == NULL ||
      frame->id != CHASSIS_CAN_ID_CMD_VELOCITY) {
    return CHASSIS_PROTOCOL_DECODE_WRONG_ID;
  }
  if (frame->dlc != CHASSIS_PROTOCOL_VELOCITY_SIZE) {
    return CHASSIS_PROTOCOL_DECODE_WRONG_LENGTH;
  }
  if (frame->data[0] != CHASSIS_PROTOCOL_SCHEMA_VERSION) {
    return CHASSIS_PROTOCOL_DECODE_VERSION;
  }
  if ((frame->data[1] & 0xFEU) != 0U || frame->data[3] != 0U ||
      frame->data[8] != 0U || frame->data[9] != 0U) {
    return CHASSIS_PROTOCOL_DECODE_RESERVED;
  }
  expected_crc = ChassisProtocol_Crc16(frame->id, frame->data, 10U);
  received_crc = (uint16_t)frame->data[10] | ((uint16_t)frame->data[11] << 8U);
  if (expected_crc != received_crc) {
    return CHASSIS_PROTOCOL_DECODE_CRC;
  }
  linear = GetI16Le(&frame->data[4]);
  if (linear < -CHASSIS_PROTOCOL_LINEAR_VELOCITY_LIMIT_MM_S ||
      linear > CHASSIS_PROTOCOL_LINEAR_VELOCITY_LIMIT_MM_S ||
      GetI16Le(&frame->data[6]) <
          -CHASSIS_PROTOCOL_ANGULAR_VELOCITY_LIMIT_MRAD_S ||
      GetI16Le(&frame->data[6]) >
          CHASSIS_PROTOCOL_ANGULAR_VELOCITY_LIMIT_MRAD_S ||
      ((frame->data[1] & 1U) == 0U &&
       (linear != 0 || GetI16Le(&frame->data[6]) != 0))) {
    return CHASSIS_PROTOCOL_DECODE_RANGE;
  }
  *command = (ChassisProtocolVelocityCommand){
      .enabled = (frame->data[1] & 1U) != 0U,
      .sequence = frame->data[2],
      .linear_velocity_mm_s = linear,
      .angular_velocity_mrad_s = GetI16Le(&frame->data[6]),
  };
  return CHASSIS_PROTOCOL_DECODE_OK;
}

ChassisProtocolDecodeResult
ChassisProtocol_DecodeHeartbeat(const struct can_frame *frame,
                                ChassisProtocolHeartbeat *heartbeat) {
  uint16_t expected_crc;
  uint16_t received_crc;

  if (frame == NULL || heartbeat == NULL ||
      frame->id != CHASSIS_CAN_ID_HEARTBEAT) {
    return CHASSIS_PROTOCOL_DECODE_WRONG_ID;
  }
  if (frame->dlc != CHASSIS_PROTOCOL_HEARTBEAT_SIZE) {
    return CHASSIS_PROTOCOL_DECODE_WRONG_LENGTH;
  }
  if (frame->data[0] != CHASSIS_PROTOCOL_SCHEMA_VERSION) {
    return CHASSIS_PROTOCOL_DECODE_VERSION;
  }
  if (frame->data[1] > CHASSIS_PROTOCOL_NODE_MAINTENANCE ||
      (frame->data[3] &
       ~CHASSIS_PROTOCOL_HEARTBEAT_FLAG_SENDER_READY) != 0U) {
    return CHASSIS_PROTOCOL_DECODE_RESERVED;
  }
  expected_crc = ChassisProtocol_Crc16(frame->id, frame->data, 10U);
  received_crc = GetU16Le(&frame->data[10]);
  if (expected_crc != received_crc) {
    return CHASSIS_PROTOCOL_DECODE_CRC;
  }
  *heartbeat = (ChassisProtocolHeartbeat){
      .node_state = (ChassisProtocolNodeState)frame->data[1],
      .sequence = frame->data[2],
      .flags = frame->data[3],
      .uptime_ms = GetU32Le(&frame->data[4]),
      .fault_summary = GetU16Le(&frame->data[8]),
  };
  return CHASSIS_PROTOCOL_DECODE_OK;
}

bool ChassisProtocol_EncodeVelocity(
    const ChassisProtocolVelocityCommand *command, struct can_frame *frame) {
  uint16_t crc;
  if (command == NULL || frame == NULL ||
      command->linear_velocity_mm_s <
          -CHASSIS_PROTOCOL_LINEAR_VELOCITY_LIMIT_MM_S ||
      command->linear_velocity_mm_s >
          CHASSIS_PROTOCOL_LINEAR_VELOCITY_LIMIT_MM_S ||
      command->angular_velocity_mrad_s <
          -CHASSIS_PROTOCOL_ANGULAR_VELOCITY_LIMIT_MRAD_S ||
      command->angular_velocity_mrad_s >
          CHASSIS_PROTOCOL_ANGULAR_VELOCITY_LIMIT_MRAD_S ||
      (!command->enabled && (command->linear_velocity_mm_s != 0 ||
                             command->angular_velocity_mrad_s != 0))) {
    return false;
  }
  *frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_CMD_VELOCITY,
      .dlc = CHASSIS_PROTOCOL_VELOCITY_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  frame->data[0] = CHASSIS_PROTOCOL_SCHEMA_VERSION;
  frame->data[1] = command->enabled ? 1U : 0U;
  frame->data[2] = command->sequence;
  PutI16Le(&frame->data[4], command->linear_velocity_mm_s);
  PutI16Le(&frame->data[6], command->angular_velocity_mrad_s);
  crc = ChassisProtocol_Crc16(frame->id, frame->data, 10U);
  frame->data[10] = (uint8_t)(crc & 0xFFU);
  frame->data[11] = (uint8_t)(crc >> 8U);
  return true;
}

bool ChassisProtocol_EncodeMotion(
    const ChassisProtocolMotionStatus *status, struct can_frame *frame) {
  if (status == NULL || frame == NULL || status->left_output_permille < -1000 ||
      status->left_output_permille > 1000 ||
      status->right_output_permille < -1000 ||
      status->right_output_permille > 1000 || status->control_state > 5U) {
    return false;
  }
  *frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_STATUS_MOTION,
      .dlc = CHASSIS_PROTOCOL_MOTION_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  frame->data[0] = CHASSIS_PROTOCOL_SCHEMA_VERSION;
  frame->data[1] =
      (status->valid ? CHASSIS_PROTOCOL_MOTION_FLAG_VALID : 0U) |
      (status->running ? CHASSIS_PROTOCOL_MOTION_FLAG_RUNNING : 0U);
  frame->data[2] = status->sequence;
  frame->data[3] = status->control_state;
  PutI16Le(&frame->data[4], status->left_velocity_mm_s);
  PutI16Le(&frame->data[6], status->right_velocity_mm_s);
  PutI16Le(&frame->data[8], status->linear_velocity_mm_s);
  PutI16Le(&frame->data[10], status->angular_velocity_mrad_s);
  PutI16Le(&frame->data[12], status->left_output_permille);
  PutI16Le(&frame->data[14], status->right_output_permille);
  return true;
}

bool ChassisProtocol_EncodeOdometry(
    const ChassisProtocolOdometryReport *report, struct can_frame *frame) {
  if (report == NULL || frame == NULL) {
    return false;
  }
  *frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_REPORT_ODOMETRY,
      .dlc = CHASSIS_PROTOCOL_ODOMETRY_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  frame->data[0] = CHASSIS_PROTOCOL_SCHEMA_VERSION;
  frame->data[1] =
      report->valid ? CHASSIS_PROTOCOL_ODOMETRY_FLAG_VALID : 0U;
  frame->data[2] = report->sequence;
  PutU32Le(&frame->data[4], report->timestamp_ms);
  PutI32Le(&frame->data[8], report->x_mm);
  PutI32Le(&frame->data[12], report->y_mm);
  PutI32Le(&frame->data[16], report->heading_mrad);
  PutI16Le(&frame->data[20], report->linear_velocity_mm_s);
  PutI16Le(&frame->data[22], report->angular_velocity_mrad_s);
  return true;
}

bool ChassisProtocol_EncodeHeartbeat(
    const ChassisProtocolHeartbeat *heartbeat, struct can_frame *frame) {
  uint16_t crc;

  if (heartbeat == NULL || frame == NULL ||
      heartbeat->node_state > CHASSIS_PROTOCOL_NODE_MAINTENANCE ||
      (heartbeat->flags & ~CHASSIS_PROTOCOL_HEARTBEAT_FLAG_SENDER_READY) !=
          0U) {
    return false;
  }
  *frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_HEARTBEAT,
      .dlc = CHASSIS_PROTOCOL_HEARTBEAT_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  frame->data[0] = CHASSIS_PROTOCOL_SCHEMA_VERSION;
  frame->data[1] = (uint8_t)heartbeat->node_state;
  frame->data[2] = heartbeat->sequence;
  frame->data[3] = heartbeat->flags;
  PutU32Le(&frame->data[4], heartbeat->uptime_ms);
  PutU16Le(&frame->data[8], heartbeat->fault_summary);
  crc = ChassisProtocol_Crc16(frame->id, frame->data, 10U);
  PutU16Le(&frame->data[10], crc);
  return true;
}

bool ChassisProtocol_EncodeFault(
    const ChassisProtocolFaultStatus *status, struct can_frame *frame) {
  uint16_t crc;

  if (status == NULL || frame == NULL ||
      status->severity > CHASSIS_PROTOCOL_FAULT_CRITICAL ||
      (status->flags & ~(CHASSIS_PROTOCOL_FAULT_FLAG_ACTIVE |
                         CHASSIS_PROTOCOL_FAULT_FLAG_CRITICAL)) != 0U) {
    return false;
  }
  *frame = (struct can_frame){
      .id = CHASSIS_CAN_ID_FAULT_STATUS,
      .dlc = CHASSIS_PROTOCOL_FAULT_SIZE,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  frame->data[0] = CHASSIS_PROTOCOL_SCHEMA_VERSION;
  frame->data[1] = (uint8_t)status->severity;
  frame->data[2] = status->sequence;
  frame->data[3] = status->flags;
  PutU32Le(&frame->data[4], status->active_faults);
  PutU32Le(&frame->data[8], status->latched_faults);
  PutU16Le(&frame->data[12], status->fault_sequence);
  crc = ChassisProtocol_Crc16(frame->id, frame->data, 14U);
  PutU16Le(&frame->data[14], crc);
  return true;
}

static int SendEncoded(bool encoded, struct can_frame *frame,
                       uint8_t *sequence) {
  int result;

  if (!encoded) {
    return -EINVAL;
  }
  if (protocol_port.send == NULL) {
    return -ENODEV;
  }
  result = protocol_port.send(frame);
  if (result == 0) {
    ++(*sequence);
  }
  return result;
}

int ChassisProtocol_SendMotion(const ChassisProtocolMotionStatus *status) {
  ChassisProtocolMotionStatus message;
  struct can_frame frame;

  if (status == NULL) {
    return -EINVAL;
  }
  message = *status;
  message.sequence = motion_tx_sequence;
  return SendEncoded(ChassisProtocol_EncodeMotion(&message, &frame), &frame,
                     &motion_tx_sequence);
}

int ChassisProtocol_SendOdometry(
    const ChassisProtocolOdometryReport *report) {
  ChassisProtocolOdometryReport message;
  struct can_frame frame;

  if (report == NULL) {
    return -EINVAL;
  }
  message = *report;
  message.sequence = odometry_tx_sequence;
  return SendEncoded(ChassisProtocol_EncodeOdometry(&message, &frame), &frame,
                     &odometry_tx_sequence);
}

int ChassisProtocol_SendHeartbeat(
    const ChassisProtocolHeartbeat *heartbeat) {
  ChassisProtocolHeartbeat message;
  struct can_frame frame;

  if (heartbeat == NULL) {
    return -EINVAL;
  }
  message = *heartbeat;
  message.sequence = heartbeat_tx_sequence;
  return SendEncoded(ChassisProtocol_EncodeHeartbeat(&message, &frame), &frame,
                     &heartbeat_tx_sequence);
}

int ChassisProtocol_SendFault(const ChassisProtocolFaultStatus *status) {
  ChassisProtocolFaultStatus message;
  struct can_frame frame;

  if (status == NULL) {
    return -EINVAL;
  }
  message = *status;
  message.sequence = fault_tx_sequence;
  return SendEncoded(ChassisProtocol_EncodeFault(&message, &frame), &frame,
                     &fault_tx_sequence);
}
