#ifndef CHASSIS_PROTOCOL_H
#define CHASSIS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/can.h"
#include "subsys/communication/can/chassis_protocol_types.h"

#define CHASSIS_PROTOCOL_SCHEMA_VERSION 1U
#define CHASSIS_PROTOCOL_WHEEL_RAW_SIZE 8U
#define CHASSIS_PROTOCOL_VELOCITY_SIZE 12U
#define CHASSIS_PROTOCOL_WHEEL_RAW_TARGET_LIMIT 100

typedef struct {
  int (*send)(const struct can_frame *frame);
} ChassisProtocolPort;

bool ChassisProtocol_Init(const ChassisProtocolPort *port);
void ChassisProtocol_ProcessFrame(const struct can_frame *frame);
void ChassisProtocol_Run(uint32_t now_ms);
void ChassisProtocol_InvalidateLink(ChassisProtocolLinkStatus status);
void ChassisProtocol_ResetLink(ChassisProtocolLinkStatus status);
void ChassisProtocol_RequestHandshakeResponse(void);
ChassisProtocolLinkStatus ChassisProtocol_GetLinkStatus(void);
bool ChassisProtocol_TakeSessionInvalidated(void);
bool ChassisProtocol_TakeWheelRawCommand(
    ChassisProtocolWheelRawCommand *command);

ChassisProtocolDecodeResult
ChassisProtocol_DecodeWheelRaw(const struct can_frame *frame,
                               ChassisProtocolWheelRawCommand *command);
ChassisProtocolDecodeResult
ChassisProtocol_DecodeVelocity(const struct can_frame *frame,
                               ChassisProtocolVelocityCommand *command);
bool ChassisProtocol_EncodeVelocity(
    const ChassisProtocolVelocityCommand *command, struct can_frame *frame);
uint16_t ChassisProtocol_Crc16(uint16_t identifier, const uint8_t *payload,
                               uint8_t length);

#endif
