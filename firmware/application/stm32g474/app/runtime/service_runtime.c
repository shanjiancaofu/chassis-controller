#include "app/runtime/service_runtime.h"

#include <stdio.h>

#include "app/chassis/command_manager.h"
#include "app/chassis/odometry.h"
#include "app/chassis/wheel_controller.h"
#include "app/console/chassis_console_commands.h"
#include "app/maintenance/chassis_maintenance.h"
#include "app/console/console.h"
#include "app/diagnostics/diagnostic_report.h"
#include "app/diagnostics/system_status.h"
#include "app/diagnostics/telemetry.h"
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
#include "drivers/reset/reset.h"
#include "drivers/time.h"
#include "drivers/uart.h"
#include "drivers/watchdog.h"
#include "kernel/critical.h"
#include "subsys/communication/can_transport/can_transport.h"
#include "subsys/communication/ota_transport/ota_can_transport.h"
#include "subsys/communication/ota_transport/ota_confirmation.h"
#include "subsys/communication/ota_transport/ota_session.h"
#include "subsys/communication/ota_transport/ota_uart_transport.h"
#include "subsys/communication/uart_protocol/uart_protocol.h"

#if CONFIG_MOTOR_DEMO
static uint8_t demo_stage;
static uint32_t demo_stage_started_ms;

static void RunMotorDemo(uint32_t now_ms)
{
  if (demo_stage < 6U) {
    kernel_critical_enter();
    (void)CommandManager_Refresh(COMMAND_SOURCE_SELF_TEST, now_ms);
    kernel_critical_exit();
  }
  switch (demo_stage) {
    case 0U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)ControlRuntime_SubmitMotionCommand(
            MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
            MOTOR_DEMO_TARGET_COUNTS_PER_TICK, COMMAND_SOURCE_SELF_TEST,
            now_ms, false, 0U);
        demo_stage = 1U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 1U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)ControlRuntime_SubmitMotionCommand(
            0, 0, COMMAND_SOURCE_SELF_TEST, now_ms, false, 0U);
        demo_stage = 2U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 2U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)ControlRuntime_SubmitMotionCommand(
            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK, COMMAND_SOURCE_SELF_TEST,
            now_ms, false, 0U);
        demo_stage = 3U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 3U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)ControlRuntime_SubmitMotionCommand(
            0, 0, COMMAND_SOURCE_SELF_TEST, now_ms, false, 0U);
        demo_stage = 4U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 4U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)ControlRuntime_SubmitMotionCommand(
            MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK, COMMAND_SOURCE_SELF_TEST,
            now_ms, false, 0U);
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

bool ServiceRuntime_Init(void)
{
#if CONFIG_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = time_uptime_ms();
  if (ControlRuntime_SubmitMotionCommand(
          0, 0, COMMAND_SOURCE_SELF_TEST, demo_stage_started_ms, false, 0U) !=
          COMMAND_SUBMIT_ACCEPTED ||
      !ControlRuntime_Start()) {
    return false;
  }
#endif
  return true;
}

void ServiceRuntime_Run(void)
{
  ConsoleCommand console_command;
  CanTransportControlCommand control_command;
  const uint32_t now_ms = time_uptime_ms();

  uart_run();
  if (OtaUartTransport_IsEnabled()) {
    OtaUartTransport_Run();
  } else {
    Console_Run();
  }
  while (Console_TakeCommand(&console_command)) {
    ChassisConsoleCommands_Process(&console_command, now_ms);
  }

#if CONFIG_MOTOR_DEMO
  RunMotorDemo(now_ms);
#endif

  CanTransport_Run();
  if (CanTransport_TakeSessionInvalidated()) {
    OtaCanTransport_Invalidate();
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
  ChassisMaintenance_Run(now_ms);
  if (CanTransport_TakeControlCommand(&control_command)) {
    if (control_command.enabled) {
      if (ControlRuntime_SubmitMotionCommand(
              control_command.left_target, control_command.right_target,
              COMMAND_SOURCE_CAN_REMOTE, now_ms, true,
              control_command.sequence) == COMMAND_SUBMIT_ACCEPTED) {
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
    OtaConfirmation_Run(
        now_ms,
        status.board_health.qspi_id_valid &&
            status.runtime.critical_tasks_healthy &&
            !FaultManager_HasCritical(),
        QspiSelfTest_GetStatus() != QSPI_SELF_TEST_RUNNING &&
            !OtaSession_IsUsingQspi() && !ParameterStorage_IsUsingQspi());
  }
  ParameterStorage_Run(
      now_ms, QspiSelfTest_GetStatus() != QSPI_SELF_TEST_RUNNING &&
                  !OtaSession_IsUsingQspi() &&
                  !OtaConfirmation_IsUsingQspi());
  {
    bool save_success;

    if (ParameterStorage_TakeCompletion(&save_success)) {
      ParameterStorageSnapshot storage;
      char fields[64];

      ParameterStorage_GetSnapshot(&storage);
      (void)snprintf(fields, sizeof(fields), "sequence=%lu error_count=%lu",
                     (unsigned long)storage.sequence,
                     (unsigned long)storage.error_count);
      (void)UartProtocol_SendLog(
          now_ms,
          save_success ? UART_PROTOCOL_LOG_INFO : UART_PROTOCOL_LOG_ERROR,
          "parameters", save_success ? "SAVED" : "SAVE_FAILED", fields);
    }
  }
  if (OtaSession_HasCriticalFault() || OtaConfirmation_HasCriticalFault() ||
      QspiSelfTest_HasCriticalFault()) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
  }
  MotorSelfTest_Run(now_ms);
  ControlRuntime_ReleaseFinishedSelfTestLock();
  if (!OtaUartTransport_IsEnabled()) {
    DiagnosticReport_Run(now_ms);
  }

  if (IwdgSelfTest_IsResetRequested()) {
    ControlRuntime_Stop();
    ControlRuntime_EmergencyStopOutputs();
  }
  if (OtaSession_IsResetRequested(now_ms) &&
      ((OtaSession_GetSource() == OTA_SOURCE_UART &&
        OtaUartTransport_IsTxIdle()) ||
       (OtaSession_GetSource() == OTA_SOURCE_CAN_FD &&
        OtaCanTransport_IsTxIdle()))) {
    ControlRuntime_Stop();
    ControlRuntime_CoastOutputs();
    if (watchdog_prepare_for_bootloader()) {
      Reset_RequestSystemReset();
    }
  }

  if (!OtaUartTransport_IsEnabled() && Telemetry_IsDue(now_ms)) {
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
