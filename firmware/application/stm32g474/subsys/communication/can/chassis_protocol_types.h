#ifndef CHASSIS_PROTOCOL_TYPES_H
#define CHASSIS_PROTOCOL_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CHASSIS_PROTOCOL_LINK_READY = 0,
  CHASSIS_PROTOCOL_LINK_PASSED,
  CHASSIS_PROTOCOL_LINK_FAILED
} ChassisProtocolLinkStatus;

typedef enum {
  CHASSIS_PROTOCOL_DECODE_OK = 0,
  CHASSIS_PROTOCOL_DECODE_WRONG_ID,
  CHASSIS_PROTOCOL_DECODE_WRONG_LENGTH,
  CHASSIS_PROTOCOL_DECODE_VERSION,
  CHASSIS_PROTOCOL_DECODE_RESERVED,
  CHASSIS_PROTOCOL_DECODE_RANGE,
  CHASSIS_PROTOCOL_DECODE_SEQUENCE,
  CHASSIS_PROTOCOL_DECODE_CRC
} ChassisProtocolDecodeResult;

typedef struct {
  bool enabled;
  uint8_t sequence;
  int16_t left_target;
  int16_t right_target;
} ChassisProtocolWheelRawCommand;

typedef struct {
  bool enabled;
  uint8_t sequence;
  int16_t linear_velocity_mm_s;
  int16_t angular_velocity_mrad_s;
} ChassisProtocolVelocityCommand;

typedef enum {
  CHASSIS_PROTOCOL_CONTROL_WHEEL_RAW = 0,
  CHASSIS_PROTOCOL_CONTROL_VELOCITY
} ChassisProtocolControlMode;

typedef struct {
  ChassisProtocolControlMode mode;
  bool enabled;
  uint8_t sequence;
  union {
    struct {
      int16_t left_target;
      int16_t right_target;
    } wheel_raw;
    struct {
      int16_t linear_velocity_mm_s;
      int16_t angular_velocity_mrad_s;
    } velocity;
  } target;
} ChassisProtocolControlCommand;

typedef enum {
  CHASSIS_PROTOCOL_NODE_STARTING = 0,
  CHASSIS_PROTOCOL_NODE_READY,
  CHASSIS_PROTOCOL_NODE_RUNNING,
  CHASSIS_PROTOCOL_NODE_DEGRADED,
  CHASSIS_PROTOCOL_NODE_FAULT,
  CHASSIS_PROTOCOL_NODE_MAINTENANCE
} ChassisProtocolNodeState;

typedef enum {
  CHASSIS_PROTOCOL_FAULT_NONE = 0,
  CHASSIS_PROTOCOL_FAULT_WARNING,
  CHASSIS_PROTOCOL_FAULT_CRITICAL
} ChassisProtocolFaultSeverity;

typedef struct {
  bool valid;
  bool running;
  uint8_t sequence;
  uint8_t control_state;
  int16_t left_velocity_mm_s;
  int16_t right_velocity_mm_s;
  int16_t linear_velocity_mm_s;
  int16_t angular_velocity_mrad_s;
  int16_t left_output_permille;
  int16_t right_output_permille;
} ChassisProtocolMotionStatus;

typedef struct {
  bool valid;
  uint8_t sequence;
  uint32_t timestamp_ms;
  int32_t x_mm;
  int32_t y_mm;
  int32_t heading_mrad;
  int16_t linear_velocity_mm_s;
  int16_t angular_velocity_mrad_s;
} ChassisProtocolOdometryReport;

typedef struct {
  ChassisProtocolNodeState node_state;
  uint8_t sequence;
  uint8_t flags;
  uint32_t uptime_ms;
  uint16_t fault_summary;
} ChassisProtocolHeartbeat;

typedef struct {
  ChassisProtocolFaultSeverity severity;
  uint8_t sequence;
  uint8_t flags;
  uint32_t active_faults;
  uint32_t latched_faults;
  uint16_t fault_sequence;
} ChassisProtocolFaultStatus;

#endif
