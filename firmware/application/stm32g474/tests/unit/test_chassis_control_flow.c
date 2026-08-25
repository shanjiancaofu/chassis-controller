#ifdef CHASSIS_CONTROL_FLOW_HOST_TEST

#include <assert.h>
#include <string.h>

#include "app/chassis/command_manager.h"
#include "app/chassis/differential_drive.h"
#include "app/safety/fault_manager.h"
#include "app/safety/safety_manager.h"
#include "config/control_config.h"
#include "subsys/communication/can/chassis_protocol.h"
#include "subsys/communication/can/chassis_protocol_ids.h"

static int Send(const struct can_frame *frame)
{
  return frame != NULL ? 0 : -1;
}

static void CompleteHandshake(void)
{
  static const uint8_t ping[8] = {'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U};
  static const uint8_t pass[8] = {'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U};
  struct can_frame frame = {
      .id = CHASSIS_CAN_ID_DEV_HANDSHAKE_REQUEST,
      .dlc = 8U,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };

  memcpy(frame.data, ping, sizeof(ping));
  ChassisProtocol_ProcessFrame(&frame);
  ChassisProtocol_Run(0U);
  memcpy(frame.data, pass, sizeof(pass));
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_GetLinkStatus() == CHASSIS_PROTOCOL_LINK_PASSED);
}

int main(void)
{
  const ChassisProtocolPort port = {.send = Send};
  ChassisProtocolVelocityCommand velocity = {
      .enabled = true,
      .sequence = 1U,
      .linear_velocity_mm_s = 500,
      .angular_velocity_mrad_s = 0,
  };
  ChassisProtocolControlCommand control;
  CommandManagerCommand stored;
  struct can_frame frame;
  int32_t left_target;
  int32_t right_target;

  CommandManager_Init();
  FaultManager_Init();
  SafetyManager_Init(false);
  assert(ChassisProtocol_Init(&port));
  CompleteHandshake();

  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(DifferentialDrive_VelocityToWheelTargets(
      control.target.velocity.linear_velocity_mm_s,
      control.target.velocity.angular_velocity_mrad_s,
      MOTOR_ENCODER_COUNTS_PER_REVOLUTION, CHASSIS_WHEEL_DIAMETER_M,
      CHASSIS_TRACK_WIDTH_M, MOTOR_CONTROL_PERIOD_MS,
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
  assert(CommandManager_IsTimedOut(1200U));

  ChassisProtocol_ProcessFrame(&frame);
  assert(!ChassisProtocol_TakeControlCommand(&control));

  velocity.sequence = 2U;
  velocity.enabled = false;
  velocity.linear_velocity_mm_s = 0;
  assert(ChassisProtocol_EncodeVelocity(&velocity, &frame));
  ChassisProtocol_ProcessFrame(&frame);
  assert(ChassisProtocol_TakeControlCommand(&control));
  assert(!control.enabled);

  FaultManager_Raise(CHASSIS_FAULT_UNDERVOLTAGE);
  SafetyManager_Stop();
  assert(!SafetyManager_RequestRun(true));
  assert(FaultManager_GetLatchedFlags() == CHASSIS_FAULT_UNDERVOLTAGE);
  return 0;
}

#endif
