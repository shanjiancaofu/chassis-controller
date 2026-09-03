#include "app/runtime/service_runtime.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "app/chassis/command_manager.h"
#include "app/chassis/differential_drive.h"
#include "app/chassis/odometry.h"
#include "app/chassis/wheel_controller.h"
#include "app/console/commands.h"
#include "app/console/console.h"
#include "app/diagnostics/diagnostic_report.h"
#include "app/diagnostics/system_status.h"
#include "app/diagnostics/telemetry.h"
#include "app/maintenance/chassis_maintenance.h"
#include "app/maintenance/self_test/iwdg_self_test.h"
#include "app/maintenance/self_test/motor_self_test.h"
#include "app/maintenance/self_test/qspi_self_test.h"
#include "app/parameters/parameter_storage.h"
#include "app/runtime/control_runtime.h"
#include "app/safety/fault_manager.h"
#include "app/safety/safety_manager.h"
#include "config/app_config.h"
#include "config/control_config.h"
#include "devicetree.h"
#include "drivers/adc/power_sample.h"
#include "drivers/motor/motor.h"
#include "drivers/reset/reset.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/time.h"
#include "drivers/uart.h"
#include "drivers/watchdog.h"
#include "kernel/critical.h"
#include "subsys/communication/can/can_transport.h"
#include "subsys/communication/can/chassis_protocol.h"
#include "subsys/communication/ota/ota_can.h"
#include "subsys/communication/ota/ota_confirmation.h"
#include "subsys/communication/ota/ota_session.h"
#include "subsys/communication/ota/ota_uart.h"
#include "subsys/communication/uart/uart_messages.h"

static uint32_t motion_tx_due_ms;
static uint32_t odometry_tx_due_ms;
static uint32_t heartbeat_tx_due_ms;
static uint32_t fault_tx_due_ms;
static uint16_t last_fault_sequence_sent;
static bool fault_sequence_sent;
static uint32_t startup_started_ms;
static uint32_t startup_candidate_started_ms;
static bool startup_candidate_asserted;
static bool startup_candidate_valid;
static bool emergency_stop_confirmation_pending;
static uint32_t emergency_stop_confirmation_started_ms;

#define EMERGENCY_STOP_STARTUP_SETTLE_MS 500U
#define EMERGENCY_STOP_STARTUP_STABLE_MS 50U
#define EMERGENCY_STOP_CONFIRM_MS 50U

static bool IsDue(uint32_t now_ms, uint32_t due_ms) {
  return (int32_t)(now_ms - due_ms) >= 0;
}

static bool RunStartupArming(uint32_t now_ms) {
  const bool asserted =
      emergency_stop_is_asserted(DEVICE_DT_GET(DT_CHOSEN(chassis_estop)));

  if (!SafetyManager_IsStarting()) {
    return true;
  }
  (void)ControlRuntime_TakeEmergencyStopEvent();
  ControlRuntime_CoastOutputs();
  if (now_ms - startup_started_ms < EMERGENCY_STOP_STARTUP_SETTLE_MS) {
    return false;
  }
  if (!startup_candidate_valid || asserted != startup_candidate_asserted) {
    startup_candidate_asserted = asserted;
    startup_candidate_started_ms = now_ms;
    startup_candidate_valid = true;
    return false;
  }
  if (now_ms - startup_candidate_started_ms <
      EMERGENCY_STOP_STARTUP_STABLE_MS) {
    return false;
  }

  SafetyManager_CompleteStartup(asserted);
  if (asserted) {
    ControlRuntime_EmergencyStopOutputs();
  } else if (!ControlRuntime_ClearEmergencyStop()) {
    SafetyManager_CompleteStartup(true);
    ControlRuntime_EmergencyStopOutputs();
  }
  return true;
}

static void RunEmergencyStopConfirmation(uint32_t now_ms) {
  const struct device *estop = DEVICE_DT_GET(DT_CHOSEN(chassis_estop));

  if (ControlRuntime_TakeEmergencyStopEvent()) {
    ControlRuntime_StopForEmergencyStopEvent();
    if (SafetyManager_IsEmergencyStopLatched()) {
      emergency_stop_confirmation_pending = false;
      ControlRuntime_EmergencyStopOutputs();
      return;
    }
    emergency_stop_confirmation_pending = true;
    emergency_stop_confirmation_started_ms = now_ms;
  }
  if (!emergency_stop_confirmation_pending) {
    return;
  }
  if (!emergency_stop_is_asserted(estop)) {
    emergency_stop_confirmation_pending = false;
    (void)ControlRuntime_ClearEmergencyStop();
    return;
  }
  if (now_ms - emergency_stop_confirmation_started_ms <
      EMERGENCY_STOP_CONFIRM_MS) {
    return;
  }
  emergency_stop_confirmation_pending = false;
  SafetyManager_LatchEmergencyStop();
  ControlRuntime_EmergencyStopOutputs();
}

static int16_t RoundToI16(float value) {
  if (!isfinite(value)) {
    return 0;
  }
  if (value >= 32766.5f) {
    return INT16_MAX;
  }
  if (value <= -32767.5f) {
    return INT16_MIN;
  }
  return (int16_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static int32_t RoundToI32(float value) {
  if (!isfinite(value)) {
    return 0;
  }
  if (value >= 2147483520.0f) {
    return INT32_MAX;
  }
  if (value <= -2147483648.0f) {
    return INT32_MIN;
  }
  return (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static int16_t WheelDeltaToMillimetersPerSecond(int32_t delta,
                                                uint32_t period_ms) {
  if (period_ms == 0U) {
    return 0;
  }
  return RoundToI16((float)delta * CHASSIS_WHEEL_DIAMETER_M *
                    3.14159265358979323846f * 1000000.0f /
                    ((float)MOTOR_ENCODER_COUNTS_PER_REVOLUTION * period_ms));
}

static int16_t DutyToPermille(int16_t duty) {
  return RoundToI16((float)duty * 1000.0f / MOTOR_COMPARE_MAX);
}

static uint8_t ProtocolControlState(ChassisControlState state) {
  switch (state) {
  case CHASSIS_CONTROL_RUNNING:
    return 1U;
  case CHASSIS_CONTROL_COMMAND_TIMEOUT:
    return 2U;
  case CHASSIS_CONTROL_EMERGENCY_STOP:
    return 3U;
  case CHASSIS_CONTROL_INTERNAL_FAULT:
    return 4U;
  case CHASSIS_CONTROL_OPEN_LOOP_TEST:
    return 5U;
  default:
    return 0U;
  }
}

static ChassisProtocolNodeState
ProtocolNodeState(const SystemStatusSnapshot *status, uint32_t active_faults,
                  bool protocol_critical_fault) {
  if (OtaSession_IsActive() || status->motor_test.running ||
      QspiSelfTest_GetStatus() == QSPI_SELF_TEST_RUNNING) {
    return CHASSIS_PROTOCOL_NODE_MAINTENANCE;
  }
  if (protocol_critical_fault) {
    return CHASSIS_PROTOCOL_NODE_FAULT;
  }
  if (active_faults != 0U || !status->runtime.critical_tasks_healthy ||
      !CanTransport_IsOperational()) {
    return CHASSIS_PROTOCOL_NODE_DEGRADED;
  }
  if (status->control_state == CHASSIS_CONTROL_RUNNING) {
    return CHASSIS_PROTOCOL_NODE_RUNNING;
  }
  return CHASSIS_PROTOCOL_NODE_READY;
}

static void PublishChassisStatus(uint32_t now_ms) {
  SystemStatusSnapshot status;
  ChassisProtocolPeerHeartbeatSnapshot peer;
  WheelControllerSnapshot wheels;
  MotorSelfTestSnapshot motor_test;
  OdometrySnapshot odometry;
  const uint32_t active_faults = FaultManager_GetFlags();
  const uint32_t latched_faults = FaultManager_GetLatchedFlags();
  const uint16_t fault_sequence = FaultManager_GetSequence();
  const bool protocol_critical_fault =
      FaultManager_HasCritical() ||
      (active_faults & CHASSIS_FAULT_EMERGENCY_STOP) != 0U;
  bool maintenance_active;

  SystemStatus_GetSnapshot(&status);
  kernel_critical_enter();
  WheelController_GetSnapshot(&wheels);
  MotorSelfTest_GetSnapshot(&motor_test);
  Odometry_GetSnapshot(now_ms, &odometry);
  status.control_state = SafetyManager_GetState();
  kernel_critical_exit();
  status.motor_test.running = motor_test.running;
  maintenance_active = OtaSession_IsActive() || motor_test.running ||
                       QspiSelfTest_GetStatus() == QSPI_SELF_TEST_RUNNING;
  ChassisProtocol_GetPeerHeartbeat(now_ms, &peer);

  if (!fault_sequence_sent || fault_sequence != last_fault_sequence_sent ||
      IsDue(now_ms, fault_tx_due_ms)) {
    const ChassisProtocolFaultStatus fault = {
        .severity = protocol_critical_fault ? CHASSIS_PROTOCOL_FAULT_CRITICAL
                    : active_faults != 0U   ? CHASSIS_PROTOCOL_FAULT_WARNING
                                            : CHASSIS_PROTOCOL_FAULT_NONE,
        .flags =
            (active_faults != 0U ? CHASSIS_PROTOCOL_FAULT_FLAG_ACTIVE : 0U) |
            (protocol_critical_fault ? CHASSIS_PROTOCOL_FAULT_FLAG_CRITICAL
                                     : 0U),
        .active_faults = active_faults,
        .latched_faults = latched_faults,
        .fault_sequence = fault_sequence,
    };

    if (ChassisProtocol_SendFault(&fault) == 0) {
      last_fault_sequence_sent = fault_sequence;
      fault_sequence_sent = true;
      fault_tx_due_ms = now_ms + CHASSIS_PROTOCOL_FAULT_PERIOD_MS;
    }
  }
  if (IsDue(now_ms, heartbeat_tx_due_ms)) {
    const ChassisProtocolHeartbeat heartbeat = {
        .node_state =
            ProtocolNodeState(&status, active_faults, protocol_critical_fault),
        .flags = CHASSIS_PROTOCOL_HEARTBEAT_FLAG_SENDER_READY,
        .uptime_ms = now_ms,
        .fault_summary = (uint16_t)active_faults,
    };

    if (ChassisProtocol_SendHeartbeat(&heartbeat) == 0) {
      heartbeat_tx_due_ms = now_ms + CHASSIS_PROTOCOL_HEARTBEAT_PERIOD_MS;
    }
  }
  if (peer.status != CHASSIS_PROTOCOL_PEER_HEARTBEAT_ALIVE) {
    motion_tx_due_ms = now_ms + CHASSIS_PROTOCOL_MOTION_PERIOD_MS;
    odometry_tx_due_ms = now_ms + CHASSIS_PROTOCOL_ODOMETRY_PERIOD_MS + 5U;
    return;
  }
  if (!maintenance_active && IsDue(now_ms, motion_tx_due_ms)) {
    const ChassisProtocolMotionStatus motion = {
        .valid = odometry.valid,
        .running = status.control_state == CHASSIS_CONTROL_RUNNING,
        .control_state = ProtocolControlState(status.control_state),
        .left_velocity_mm_s = WheelDeltaToMillimetersPerSecond(
            wheels.left_measurement, MOTOR_CONTROL_REFERENCE_PERIOD_MS),
        .right_velocity_mm_s = WheelDeltaToMillimetersPerSecond(
            wheels.right_measurement, MOTOR_CONTROL_REFERENCE_PERIOD_MS),
        .linear_velocity_mm_s =
            RoundToI16(odometry.linear_velocity_mps * 1000.0f),
        .angular_velocity_mrad_s =
            RoundToI16(odometry.angular_velocity_rad_s * 1000.0f),
        .left_output_permille = DutyToPermille(
            motor_test.running ? motor_test.left_duty : wheels.left_output),
        .right_output_permille = DutyToPermille(
            motor_test.running ? motor_test.right_duty : wheels.right_output),
    };

    if (ChassisProtocol_SendMotion(&motion) == 0) {
      motion_tx_due_ms = now_ms + CHASSIS_PROTOCOL_MOTION_PERIOD_MS;
    }
  } else if (!maintenance_active && IsDue(now_ms, odometry_tx_due_ms)) {
    const ChassisProtocolOdometryReport report = {
        .valid = odometry.valid,
        .timestamp_ms = odometry.sample_timestamp_ms,
        .x_mm = RoundToI32(odometry.x_m * 1000.0f),
        .y_mm = RoundToI32(odometry.y_m * 1000.0f),
        .heading_mrad = RoundToI32(odometry.heading_rad * 1000.0f),
        .linear_velocity_mm_s =
            RoundToI16(odometry.linear_velocity_mps * 1000.0f),
        .angular_velocity_mrad_s =
            RoundToI16(odometry.angular_velocity_rad_s * 1000.0f),
    };

    if (ChassisProtocol_SendOdometry(&report) == 0) {
      odometry_tx_due_ms = now_ms + CHASSIS_PROTOCOL_ODOMETRY_PERIOD_MS;
    }
  }
}

#if CONFIG_MOTOR_DEMO
static uint8_t demo_stage;
static uint32_t demo_stage_started_ms;

static void RunMotorDemo(uint32_t now_ms) {
  if (demo_stage < 6U) {
    kernel_critical_enter();
    (void)CommandManager_Refresh(COMMAND_SOURCE_SELF_TEST, now_ms);
    kernel_critical_exit();
  }
  switch (demo_stage) {
  case 0U:
    if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
      (void)ControlRuntime_SubmitMotionCommand(
          MOTOR_DEMO_TARGET_COUNTS_PER_TICK, MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
          COMMAND_SOURCE_SELF_TEST, now_ms, false, 0U);
      demo_stage = 1U;
      demo_stage_started_ms = now_ms;
    }
    break;
  case 1U:
    if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
      (void)ControlRuntime_SubmitMotionCommand(0, 0, COMMAND_SOURCE_SELF_TEST,
                                               now_ms, false, 0U);
      demo_stage = 2U;
      demo_stage_started_ms = now_ms;
    }
    break;
  case 2U:
    if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
      (void)ControlRuntime_SubmitMotionCommand(
          -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
          -MOTOR_DEMO_TARGET_COUNTS_PER_TICK, COMMAND_SOURCE_SELF_TEST, now_ms,
          false, 0U);
      demo_stage = 3U;
      demo_stage_started_ms = now_ms;
    }
    break;
  case 3U:
    if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
      (void)ControlRuntime_SubmitMotionCommand(0, 0, COMMAND_SOURCE_SELF_TEST,
                                               now_ms, false, 0U);
      demo_stage = 4U;
      demo_stage_started_ms = now_ms;
    }
    break;
  case 4U:
    if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
      (void)ControlRuntime_SubmitMotionCommand(
          MOTOR_DEMO_TARGET_COUNTS_PER_TICK, -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
          COMMAND_SOURCE_SELF_TEST, now_ms, false, 0U);
      demo_stage = 5U;
      demo_stage_started_ms = now_ms;
    }
    break;
  case 5U:
    if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
      ControlRuntime_Stop();
      kernel_critical_enter();
      CommandManager_Release(COMMAND_SOURCE_SELF_TEST);
      kernel_critical_exit();
      demo_stage = 6U;
    }
    break;
  default:
    break;
  }
}
#endif

bool ServiceRuntime_Init(void) {
  const uint32_t now_ms = time_uptime_ms();

  motion_tx_due_ms = now_ms + CHASSIS_PROTOCOL_MOTION_PERIOD_MS;
  odometry_tx_due_ms = now_ms + CHASSIS_PROTOCOL_ODOMETRY_PERIOD_MS + 5U;
  heartbeat_tx_due_ms = now_ms + CHASSIS_PROTOCOL_HEARTBEAT_PERIOD_MS + 10U;
  fault_tx_due_ms = now_ms + CHASSIS_PROTOCOL_FAULT_PERIOD_MS + 15U;
  last_fault_sequence_sent = 0U;
  fault_sequence_sent = false;
  startup_started_ms = now_ms;
  startup_candidate_started_ms = 0U;
  startup_candidate_asserted = false;
  startup_candidate_valid = false;
  emergency_stop_confirmation_pending = false;
  emergency_stop_confirmation_started_ms = 0U;
#if CONFIG_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = now_ms;
  if (ControlRuntime_SubmitMotionCommand(0, 0, COMMAND_SOURCE_SELF_TEST,
                                         demo_stage_started_ms, false,
                                         0U) != COMMAND_SUBMIT_ACCEPTED ||
      !ControlRuntime_Start()) {
    return false;
  }
#endif
  return true;
}

void ServiceRuntime_Run(void) {
  ConsoleCommand console_command;
  ChassisProtocolControlCommand control_command;
  struct can_frame frame;
  const uint32_t now_ms = time_uptime_ms();

  uart_run();
  if (!RunStartupArming(now_ms)) {
    return;
  }
  RunEmergencyStopConfirmation(now_ms);
  if (OtaUart_IsEnabled()) {
    OtaUart_Run();
  } else {
    Console_Run();
  }
  while (Console_TakeCommand(&console_command)) {
    ConsoleCommands_Process(&console_command, now_ms);
  }

#if CONFIG_MOTOR_DEMO
  RunMotorDemo(now_ms);
#endif

  while (CanTransport_Receive(&frame) == 0) {
    if (frame.id == OTA_CAN_REQUEST_ID) {
      (void)OtaCan_OnRxFrame(&frame);
    } else {
      ChassisProtocol_ProcessFrame(&frame, now_ms);
    }
  }
  CanTransport_Run();
  if (CanTransport_TakeSessionInvalidated()) {
    ChassisProtocol_InvalidateTransport();
  }
  if (CanTransport_TakeRecovered()) {
    ChassisProtocol_ResetTransport();
  }
  if (ChassisProtocol_TakeTransportInvalidated()) {
    OtaCan_Invalidate();
    OtaSession_AbortSource(OTA_SOURCE_CAN_FD, now_ms);
    kernel_critical_enter();
    if (CommandManager_GetOwner() == COMMAND_SOURCE_CAN_REMOTE) {
      kernel_critical_exit();
      ControlRuntime_Stop();
      kernel_critical_enter();
      CommandManager_Release(COMMAND_SOURCE_CAN_REMOTE);
      kernel_critical_exit();
    } else {
      kernel_critical_exit();
    }
  }
  ChassisProtocol_Run(now_ms);
  PublishChassisStatus(now_ms);
  ChassisMaintenance_Run(now_ms);
  if (ChassisProtocol_TakeControlCommand(&control_command)) {
    if (control_command.enabled) {
      int32_t left_target;
      int32_t right_target;
      bool target_valid = true;

      if (control_command.mode == CHASSIS_PROTOCOL_CONTROL_VELOCITY) {
        target_valid = DifferentialDrive_VelocityToWheelTargets(
            control_command.target.velocity.linear_velocity_mm_s,
            control_command.target.velocity.angular_velocity_mrad_s,
            MOTOR_ENCODER_COUNTS_PER_REVOLUTION, CHASSIS_WHEEL_DIAMETER_M,
            CHASSIS_TRACK_WIDTH_M, MOTOR_CONTROL_REFERENCE_PERIOD_MS,
            MOTOR_CONTROL_TARGET_LIMIT, &left_target, &right_target);
      } else {
        left_target = control_command.target.wheel_raw.left_target;
        right_target = control_command.target.wheel_raw.right_target;
      }
      if (target_valid &&
          ControlRuntime_SubmitMotionCommand(
              left_target, right_target, COMMAND_SOURCE_CAN_REMOTE, now_ms,
              true, control_command.sequence) == COMMAND_SUBMIT_ACCEPTED) {
        (void)ControlRuntime_Start();
      }
    } else if (CommandManager_GetOwner() == COMMAND_SOURCE_CAN_REMOTE) {
      ControlRuntime_Stop();
      kernel_critical_enter();
      CommandManager_Release(COMMAND_SOURCE_CAN_REMOTE);
      kernel_critical_exit();
    }
  }

  QspiSelfTest_Run(now_ms);
  {
    SystemStatusSnapshot status;

    SystemStatus_GetSnapshot(&status);
    OtaConfirmation_Run(now_ms,
                        status.board_health.qspi_id_valid &&
                            status.runtime.critical_tasks_healthy &&
                            !FaultManager_HasCritical(),
                        QspiSelfTest_GetStatus() != QSPI_SELF_TEST_RUNNING &&
                            !OtaSession_IsUsingQspi() &&
                            !ParameterStorage_IsUsingQspi());
  }
  ParameterStorage_Run(
      now_ms, QspiSelfTest_GetStatus() != QSPI_SELF_TEST_RUNNING &&
                  !OtaSession_IsUsingQspi() && !OtaConfirmation_IsUsingQspi());
  {
    bool save_success;

    if (ParameterStorage_TakeCompletion(&save_success)) {
      ParameterStorageSnapshot storage;
      char fields[64];

      ParameterStorage_GetSnapshot(&storage);
      (void)snprintf(fields, sizeof(fields), "sequence=%lu error_count=%lu",
                     (unsigned long)storage.sequence,
                     (unsigned long)storage.error_count);
      (void)UartMessages_SendLog(
          now_ms,
          save_success ? UART_MESSAGES_LOG_INFO : UART_MESSAGES_LOG_ERROR,
          "parameters", save_success ? "SAVED" : "SAVE_FAILED", fields);
    }
  }
  if (OtaSession_HasCriticalFault() || OtaConfirmation_HasCriticalFault() ||
      QspiSelfTest_HasCriticalFault()) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
  }
  MotorSelfTest_Run(now_ms);
  ControlRuntime_ReleaseFinishedSelfTestLock();
  if (!OtaUart_IsEnabled()) {
    DiagnosticReport_Run(now_ms);
  }

  if (IwdgSelfTest_IsResetRequested()) {
    ControlRuntime_Stop();
    ControlRuntime_EmergencyStopOutputs();
  }
  if (OtaSession_IsResetRequested(now_ms) &&
      ((OtaSession_GetSource() == OTA_SOURCE_UART && OtaUart_IsTxIdle()) ||
       (OtaSession_GetSource() == OTA_SOURCE_CAN_FD && OtaCan_IsTxIdle()))) {
    ControlRuntime_Stop();
    ControlRuntime_CoastOutputs();
    if (watchdog_prepare_for_bootloader()) {
      Reset_RequestSystemReset();
    }
  }

  if (!OtaUart_IsEnabled() && Telemetry_IsDue(now_ms)) {
    WheelControllerSnapshot wheel_snapshot;
    MotorSelfTestSnapshot motor_test_snapshot;
    OdometrySnapshot odometry_snapshot;
    TelemetrySnapshot snapshot;
    uint32_t supply_mv;
    const bool supply_valid = power_sample_read_millivolts(
        DEVICE_DT_GET(DT_CHOSEN(chassis_power)), &supply_mv);

    kernel_critical_enter();
    WheelController_GetSnapshot(&wheel_snapshot);
    MotorSelfTest_GetSnapshot(&motor_test_snapshot);
    Odometry_GetSnapshot(now_ms, &odometry_snapshot);
    kernel_critical_exit();
    snapshot.left_target = wheel_snapshot.left_target;
    snapshot.left_delta = odometry_snapshot.left_delta;
    snapshot.left_total = odometry_snapshot.left_total;
    snapshot.left_output = motor_test_snapshot.running
                               ? motor_test_snapshot.left_duty
                               : wheel_snapshot.left_output;
    snapshot.right_target = wheel_snapshot.right_target;
    snapshot.right_delta = odometry_snapshot.right_delta;
    snapshot.right_total = odometry_snapshot.right_total;
    snapshot.right_output = motor_test_snapshot.running
                                ? motor_test_snapshot.right_duty
                                : wheel_snapshot.right_output;
    snapshot.odometry_valid = odometry_snapshot.valid;
    snapshot.odometry_sample_timestamp_ms =
        odometry_snapshot.sample_timestamp_ms;
    snapshot.odometry_sample_age_ms = odometry_snapshot.sample_age_ms;
    snapshot.odometry_x_m = odometry_snapshot.x_m;
    snapshot.odometry_y_m = odometry_snapshot.y_m;
    snapshot.odometry_heading_rad = odometry_snapshot.heading_rad;
    snapshot.odometry_linear_velocity_mps =
        odometry_snapshot.linear_velocity_mps;
    snapshot.odometry_angular_velocity_rad_s =
        odometry_snapshot.angular_velocity_rad_s;
    snapshot.supply_mv = supply_valid ? (int32_t)supply_mv : -1;
    snapshot.control_state = (uint32_t)SafetyManager_GetState();
    snapshot.fault_flags = FaultManager_GetFlags();
    Telemetry_Run(now_ms, &snapshot);
  }
}
