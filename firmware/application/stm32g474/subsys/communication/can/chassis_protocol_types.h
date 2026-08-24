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

#endif
