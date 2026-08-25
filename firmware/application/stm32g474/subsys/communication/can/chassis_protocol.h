#ifndef CHASSIS_PROTOCOL_H
#define CHASSIS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/can.h"
#include "subsys/communication/can/chassis_protocol_types.h"

#define CHASSIS_PROTOCOL_SCHEMA_VERSION 1U
#define CHASSIS_PROTOCOL_WHEEL_RAW_SIZE 8U
#define CHASSIS_PROTOCOL_VELOCITY_SIZE 12U
#define CHASSIS_PROTOCOL_MOTION_SIZE 16U
#define CHASSIS_PROTOCOL_ODOMETRY_SIZE 24U
#define CHASSIS_PROTOCOL_HEARTBEAT_SIZE 12U
#define CHASSIS_PROTOCOL_FAULT_SIZE 16U
#define CHASSIS_PROTOCOL_MOTION_PERIOD_MS 20U
#define CHASSIS_PROTOCOL_ODOMETRY_PERIOD_MS 20U
#define CHASSIS_PROTOCOL_HEARTBEAT_PERIOD_MS 100U
#define CHASSIS_PROTOCOL_HEARTBEAT_TIMEOUT_MS 300U
#define CHASSIS_PROTOCOL_FAULT_PERIOD_MS 100U
#define CHASSIS_PROTOCOL_WHEEL_RAW_TARGET_LIMIT 100
#define CHASSIS_PROTOCOL_LINEAR_VELOCITY_LIMIT_MM_S 2000
#define CHASSIS_PROTOCOL_ANGULAR_VELOCITY_LIMIT_MRAD_S 10000

#define CHASSIS_PROTOCOL_MOTION_FLAG_VALID (1U << 0)
#define CHASSIS_PROTOCOL_MOTION_FLAG_RUNNING (1U << 1)
#define CHASSIS_PROTOCOL_ODOMETRY_FLAG_VALID (1U << 0)
#define CHASSIS_PROTOCOL_HEARTBEAT_FLAG_SENDER_READY (1U << 0)
#define CHASSIS_PROTOCOL_FAULT_FLAG_ACTIVE (1U << 0)
#define CHASSIS_PROTOCOL_FAULT_FLAG_CRITICAL (1U << 1)

typedef struct {
  int (*send)(const struct can_frame *frame);
} ChassisProtocolPort;

bool ChassisProtocol_Init(const ChassisProtocolPort *port);
void ChassisProtocol_ProcessFrame(const struct can_frame *frame,
                                  uint32_t now_ms);
void ChassisProtocol_Run(uint32_t now_ms);
void ChassisProtocol_InvalidateTransport(void);
void ChassisProtocol_ResetTransport(void);
void ChassisProtocol_RequestHandshakeResponse(void);
ChassisProtocolDevelopmentHandshakeStatus
ChassisProtocol_GetDevelopmentHandshakeStatus(void);
void ChassisProtocol_GetPeerHeartbeat(
    uint32_t now_ms, ChassisProtocolPeerHeartbeatSnapshot *snapshot);
bool ChassisProtocol_TakeTransportInvalidated(void);
bool ChassisProtocol_TakeControlCommand(
    ChassisProtocolControlCommand *command);

ChassisProtocolDecodeResult
ChassisProtocol_DecodeWheelRaw(const struct can_frame *frame,
                               ChassisProtocolWheelRawCommand *command);
ChassisProtocolDecodeResult
ChassisProtocol_DecodeVelocity(const struct can_frame *frame,
                               ChassisProtocolVelocityCommand *command);
ChassisProtocolDecodeResult
ChassisProtocol_DecodeHeartbeat(const struct can_frame *frame,
                                ChassisProtocolHeartbeat *heartbeat);
bool ChassisProtocol_EncodeVelocity(
    const ChassisProtocolVelocityCommand *command, struct can_frame *frame);
bool ChassisProtocol_EncodeMotion(
    const ChassisProtocolMotionStatus *status, struct can_frame *frame);
bool ChassisProtocol_EncodeOdometry(
    const ChassisProtocolOdometryReport *report, struct can_frame *frame);
bool ChassisProtocol_EncodeHeartbeat(
    const ChassisProtocolHeartbeat *heartbeat, struct can_frame *frame);
bool ChassisProtocol_EncodeFault(
    const ChassisProtocolFaultStatus *status, struct can_frame *frame);
int ChassisProtocol_SendMotion(const ChassisProtocolMotionStatus *status);
int ChassisProtocol_SendOdometry(
    const ChassisProtocolOdometryReport *report);
int ChassisProtocol_SendHeartbeat(
    const ChassisProtocolHeartbeat *heartbeat);
int ChassisProtocol_SendFault(const ChassisProtocolFaultStatus *status);
uint16_t ChassisProtocol_Crc16(uint16_t identifier, const uint8_t *payload,
                               uint8_t length);

#endif
