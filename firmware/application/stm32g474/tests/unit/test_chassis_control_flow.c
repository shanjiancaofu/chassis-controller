#ifdef CHASSIS_CONTROL_FLOW_HOST_TEST

#include <assert.h>

#include "app/chassis/command_manager.h"
#include "app/chassis/differential_drive.h"
#include "app/safety/fault_manager.h"
#include "app/safety/safety_manager.h"
#include "config/control_config.h"
#include "subsys/communication/can/chassis_protocol.h"
#include "subsys/communication/can/chassis_protocol_ids.h"

static int Send(const struct can_frame *frame) {
  return frame != NULL ? 0 : -1;
}

int main(void) {
  const ChassisProtocolPort port = {.send = Send};
  ChassisProtocolVelocityCommand velocity = {
      .enabled = true,
      .sequence = 1U,
      .linear_velocity_mm_s = 500,
      .angular_velocity_mrad_s = 0,
  };
  ChassisProtocolControlCommand control;
  CommandManagerCommand stored;
  ChassisProtocolHeartbeat heartbeat = {
      .node_state = CHASSIS_PROTOCOL_NODE_RUNNING,
      .sequence = 1U,
      .flags = CHASSIS_PROTOCOL_HEARTBEAT_FLAG_SENDER_READY,
      .uptime_ms = 1150U,
  };
  struct can_frame frame;
  int32_t left_target;
  int32_t right_target;

  CommandManager_Init();
  FaultManager_Init();
  SafetyManager_Init(false);
  assert(ChassisProtocol_Init(&port));

  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  ChassisProtocol_ProcessFrame(&frame, 1000U);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(DifferentialDrive_VelocityToWheelTargets(
      control.target.velocity.linear_velocity_mm_s,
      control.target.velocity.angular_velocity_mrad_s,
      MOTOR_ENCODER_COUNTS_PER_REVOLUTION, CHASSIS_WHEEL_DIAMETER_M,
      CHASSIS_TRACK_WIDTH_M, MOTOR_CONTROL_REFERENCE_PERIOD_MS,
      MOTOR_CONTROL_TARGET_LIMIT, &left_target, &right_target));
  assert(left_target == 32 && right_target == 32);
  assert(CommandManager_Submit(&(CommandManagerCommand){
             .left_target = left_target,
             .right_target = right_target,
             .received_ms = 1000U,
             .source = COMMAND_SOURCE_CAN_REMOTE,
             .sequence = control.sequence,
             .has_sequence = true,
         }) == COMMAND_SUBMIT_ACCEPTED);
  assert(SafetyManager_RequestRun(true));
  assert(CommandManager_Get(&stored));
  assert(stored.left_target == 32 && stored.right_target == 32);
  assert(!CommandManager_IsTimedOut(1199U));
  assert(ChassisProtocol_EncodeHeartbeat(&heartbeat, &frame));
  ChassisProtocol_ProcessFrame(&frame, 1150U);
  assert(!ChassisProtocol_TakeControlCommand(&control));
  assert(CommandManager_IsTimedOut(1200U));

  velocity.sequence = 0U;
  velocity.enabled = false;
  velocity.linear_velocity_mm_s = 0;
  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  ChassisProtocol_ProcessFrame(&frame, 1201U);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(!control.enabled);

  FaultManager_Raise(CHASSIS_FAULT_UNDERVOLTAGE);
  SafetyManager_Stop();
  assert(!SafetyManager_RequestRun(true));
  assert(FaultManager_GetLatchedFlags() == CHASSIS_FAULT_UNDERVOLTAGE);
  return 0;
}

#endif
