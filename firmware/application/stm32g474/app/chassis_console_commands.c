#include "app/chassis_console_commands.h"

#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "communication/can_transport/can_transport.h"
#include "config/control_config.h"
#include "infrastructure/console/diagnostic_report.h"
#include "infrastructure/parameter_storage/parameter_storage.h"
#include "infrastructure/telemetry/telemetry.h"
#include "infrastructure/uart_protocol/uart_protocol.h"
#include "modules/chassis/odometry.h"
#include "modules/parameters/parameter_manager.h"
#include "modules/safety/safety_manager.h"
#include "task.h"
#include "tests/target/iwdg_target_test.h"
#include "tests/target/qspi_target_test.h"

static ChassisConsoleCommandPort command_port;

static void SendPidParameterReport(uint32_t now_ms, const char *command_name,
                                   bool accepted);
static void SendEncoderResult(uint32_t now_ms);
static void SendCanDiagnostics(uint32_t now_ms);
static void SendConsoleHelp(uint32_t now_ms);
static const char *ParameterPersistenceText(
    const ParameterStorageSnapshot *snapshot);
static const char *MotionCommandErrorCode(
    CommandManagerSubmitResult submit_result);

bool ChassisConsoleCommands_Init(const ChassisConsoleCommandPort *port)
{
  if (port == NULL || port->submit_motion_command == NULL ||
      port->start_control == NULL || port->stop_control == NULL ||
      port->reset_wheel_odometry == NULL || port->arm_uart_ota == NULL ||
      port->acquire_target_test_lock == NULL ||
      port->start_motor_target_test == NULL) {
    return false;
  }
  command_port = *port;
  return true;
}

void ChassisConsoleCommands_Process(const ConsoleCommand *command,
                                    uint32_t now_ms)
{
  if (command == NULL) {
    return;
  }

  switch (command->type) {
    case CONSOLE_COMMAND_PING:
      (void)UartProtocol_SendResponse(now_ms, "ping", true, NULL);
      break;
    case CONSOLE_COMMAND_STATUS:
      (void)UartProtocol_SendResponse(now_ms, "status", true,
                                      "stream=diagnostics");
      DiagnosticReport_RequestSelfTest();
      break;
    case CONSOLE_COMMAND_TELEMETRY_TEXT:
      Telemetry_SetMode(TELEMETRY_MODE_TEXT);
      (void)UartProtocol_SendResponse(now_ms, "telemetry", true,
                                      "mode=TEXT");
      break;
    case CONSOLE_COMMAND_TELEMETRY_VOFA:
      Telemetry_SetMode(TELEMETRY_MODE_VOFA);
      (void)UartProtocol_SendResponse(now_ms, "telemetry", true,
                                      "mode=VOFA compatibility=LEGACY");
      break;
    case CONSOLE_COMMAND_TELEMETRY_OFF:
      Telemetry_SetMode(TELEMETRY_MODE_OFF);
      (void)UartProtocol_SendResponse(now_ms, "telemetry", true,
                                      "mode=OFF");
      break;
    case CONSOLE_COMMAND_CAN_STATUS:
      SendCanDiagnostics(now_ms);
      break;
    case CONSOLE_COMMAND_CAN_TRANSMIT:
      CanTransport_RequestResponse();
      (void)UartProtocol_SendResponse(now_ms, "can_tx", true,
                                      "frame=0x721 state=QUEUED");
      break;
    case CONSOLE_COMMAND_PID_SHOW:
      SendPidParameterReport(now_ms, "pid_show", true);
      break;
    case CONSOLE_COMMAND_PID_SET_LEFT:
    case CONSOLE_COMMAND_PID_SET_RIGHT: {
      bool accepted;
      ParameterSnapshot requested_parameters;

      taskENTER_CRITICAL();
      accepted = ParameterManager_StagePidGains(
          command->type == CONSOLE_COMMAND_PID_SET_LEFT
              ? PARAMETER_WHEEL_LEFT
              : PARAMETER_WHEEL_RIGHT,
          command->arguments.pid.kp, command->arguments.pid.ki,
          command->arguments.pid.kd);
      ParameterManager_GetRequested(&requested_parameters);
      taskEXIT_CRITICAL();
      if (accepted) {
        accepted = ParameterStorage_RequestSave(&requested_parameters);
      }
      SendPidParameterReport(now_ms, "pid_set", accepted);
      break;
    }
    case CONSOLE_COMMAND_PID_TARGET: {
      char fields[32];
      const CommandManagerSubmitResult submit_result =
          command_port.submit_motion_command(
              command->arguments.target.left,
              command->arguments.target.right, COMMAND_SOURCE_CONSOLE,
              now_ms, false, 0U);

      if (submit_result == COMMAND_SUBMIT_ACCEPTED &&
          command_port.start_control()) {
        (void)UartProtocol_SendResponse(now_ms, "pid_target", true,
                                        "state=RUNNING");
      } else {
        if (submit_result == COMMAND_SUBMIT_ACCEPTED) {
          taskENTER_CRITICAL();
          CommandManager_Release(COMMAND_SOURCE_CONSOLE);
          taskEXIT_CRITICAL();
        }
        (void)snprintf(fields, sizeof(fields), "code=%s",
                       MotionCommandErrorCode(submit_result));
        (void)UartProtocol_SendResponse(now_ms, "pid_target", false, fields);
      }
      break;
    }
    case CONSOLE_COMMAND_PID_STOP:
      command_port.stop_control();
      taskENTER_CRITICAL();
      CommandManager_Release(COMMAND_SOURCE_CONSOLE);
      taskEXIT_CRITICAL();
      (void)UartProtocol_SendResponse(now_ms, "pid_stop", true,
                                      "state=STOPPED");
      break;
    case CONSOLE_COMMAND_ENCODER_ZERO: {
      const bool accepted = command_port.reset_wheel_odometry();

      (void)UartProtocol_SendResponse(now_ms, "encoder_zero", accepted,
                                      accepted ? "state=RESET"
                                               : "code=SAFETY_STOP");
      break;
    }
    case CONSOLE_COMMAND_ENCODER_RESULT:
      SendEncoderResult(now_ms);
      break;
    case CONSOLE_COMMAND_ODOMETRY_RESET: {
      const bool accepted = command_port.reset_wheel_odometry();

      (void)UartProtocol_SendResponse(now_ms, "odometry_reset", accepted,
                                      accepted ? "state=RESET"
                                               : "code=SAFETY_STOP");
      break;
    }
    case CONSOLE_COMMAND_OTA_UART:
      if (command_port.arm_uart_ota(now_ms)) {
        (void)UartProtocol_SendResponse(now_ms, "ota_uart", true,
                                        "mode=BINARY");
      } else {
        (void)UartProtocol_SendResponse(now_ms, "ota_uart", false,
                                        "code=BUSY");
      }
      break;
    case CONSOLE_COMMAND_QSPI_TEST:
      if (!command_port.acquire_target_test_lock() ||
          !QspiTargetTest_Start()) {
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        (void)UartProtocol_SendResponse(now_ms, "qspi_test", false,
                                        "code=BUSY");
      } else {
        (void)UartProtocol_SendResponse(now_ms, "qspi_test", true,
                                        "state=STARTED");
        DiagnosticReport_RequestQspiTest();
      }
      break;
    case CONSOLE_COMMAND_IWDG_RESET:
      if (!command_port.acquire_target_test_lock() ||
          !IwdgTargetTest_Start()) {
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        (void)UartProtocol_SendResponse(now_ms, "iwdg_reset_test", false,
                                        "code=BUSY");
      } else {
        (void)UartProtocol_SendResponse(now_ms, "iwdg_reset_test", true,
                                        "state=ARMED");
        DiagnosticReport_RequestIwdgArmed();
      }
      break;
    case CONSOLE_COMMAND_MOTOR_DUTY: {
      const bool accepted =
          MotorTargetTest_SetDuty(command->arguments.motor_duty);
      char fields[48];

      (void)snprintf(fields, sizeof(fields), "duty=%u",
                     (unsigned int)MotorTargetTest_GetDuty());
      (void)UartProtocol_SendResponse(now_ms, "motor_duty", accepted,
                                      accepted ? fields : "code=RUNNING");
      break;
    }
    case CONSOLE_COMMAND_MOTOR_STOP:
    case CONSOLE_COMMAND_MOTOR_LEFT_FORWARD:
    case CONSOLE_COMMAND_MOTOR_LEFT_REVERSE:
    case CONSOLE_COMMAND_MOTOR_RIGHT_FORWARD:
    case CONSOLE_COMMAND_MOTOR_RIGHT_REVERSE: {
      const MotorTargetTestAction action =
          command->type == CONSOLE_COMMAND_MOTOR_STOP
              ? MOTOR_TARGET_TEST_STOP
              : command->type == CONSOLE_COMMAND_MOTOR_LEFT_FORWARD
                    ? MOTOR_TARGET_TEST_LEFT_FORWARD
                    : command->type == CONSOLE_COMMAND_MOTOR_LEFT_REVERSE
                          ? MOTOR_TARGET_TEST_LEFT_REVERSE
                          : command->type == CONSOLE_COMMAND_MOTOR_RIGHT_FORWARD
                                ? MOTOR_TARGET_TEST_RIGHT_FORWARD
                                : MOTOR_TARGET_TEST_RIGHT_REVERSE;
      bool accepted;

      if (action == MOTOR_TARGET_TEST_STOP) {
        command_port.stop_control();
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        accepted = true;
      } else {
        accepted = command_port.acquire_target_test_lock() &&
                   command_port.start_motor_target_test(action, now_ms);
        if (!accepted) {
          taskENTER_CRITICAL();
          CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
          taskEXIT_CRITICAL();
        }
      }
      (void)UartProtocol_SendResponse(
          now_ms,
          action == MOTOR_TARGET_TEST_STOP
              ? "motor_stop"
              : action == MOTOR_TARGET_TEST_LEFT_FORWARD
                    ? "motor_left_forward"
              : action == MOTOR_TARGET_TEST_LEFT_REVERSE
                          ? "motor_left_reverse"
                          : action == MOTOR_TARGET_TEST_RIGHT_FORWARD
                                ? "motor_right_forward"
                                : "motor_right_reverse",
          accepted,
          accepted ? (action == MOTOR_TARGET_TEST_STOP ? "state=STOPPED"
                                                       : "state=STARTED")
                   : "code=BUSY");
      break;
    }
    case CONSOLE_COMMAND_HELP:
      SendConsoleHelp(now_ms);
      break;
    case CONSOLE_COMMAND_INVALID:
      (void)UartProtocol_SendResponse(now_ms, "unknown", false,
                                      "code=INVALID_ARGUMENT");
      break;
    default:
      break;
  }
}

static void SendPidParameterReport(uint32_t now_ms, const char *command_name,
                                   bool accepted)
{
  char fields[192];
  ParameterSnapshot parameters;
  ParameterStorageSnapshot storage;

  taskENTER_CRITICAL();
  ParameterManager_GetRequested(&parameters);
  taskEXIT_CRITICAL();
  ParameterStorage_GetSnapshot(&storage);
  if (accepted) {
    (void)snprintf(
        fields, sizeof(fields),
        "left_kp=%u left_ki=%u left_kd=%u right_kp=%u right_ki=%u "
        "right_kd=%u persistence=%s sequence=%lu",
        (unsigned int)parameters.left_pid.kp,
        (unsigned int)parameters.left_pid.ki,
        (unsigned int)parameters.left_pid.kd,
        (unsigned int)parameters.right_pid.kp,
        (unsigned int)parameters.right_pid.ki,
        (unsigned int)parameters.right_pid.kd,
        ParameterPersistenceText(&storage),
        (unsigned long)storage.sequence);
    (void)UartProtocol_SendResponse(now_ms, command_name, true, fields);
  } else {
    (void)snprintf(fields, sizeof(fields),
                   "code=INVALID_ARGUMENT kp_max=%u ki_max=%u kd_max=%u",
                   (unsigned int)MOTOR_PID_KP_MAX,
                   (unsigned int)MOTOR_PID_KI_MAX,
                   (unsigned int)MOTOR_PID_KD_MAX);
    (void)UartProtocol_SendResponse(now_ms, "pid_set", false, fields);
  }
}

static void SendEncoderResult(uint32_t now_ms)
{
  OdometrySnapshot snapshot;
  char fields[96];
  char left_total[24];
  char right_total[24];

  taskENTER_CRITICAL();
  Odometry_GetSnapshot(now_ms, &snapshot);
  taskEXIT_CRITICAL();
  (void)UartProtocol_FormatSigned64(left_total, sizeof(left_total),
                                    snapshot.left_total);
  (void)UartProtocol_FormatSigned64(right_total, sizeof(right_total),
                                    snapshot.right_total);
  (void)snprintf(fields, sizeof(fields), "left_total=%s right_total=%s",
                 left_total, right_total);
  (void)UartProtocol_SendResponse(now_ms, "encoder_result", true, fields);
}

static void SendCanDiagnostics(uint32_t now_ms)
{
  CanTransportDiagnostics diagnostics;
  char fields[640];

  if (!CanTransport_GetDiagnostics(&diagnostics)) {
    (void)UartProtocol_SendResponse(now_ms, "can_status", false,
                                    "code=UNAVAILABLE");
    return;
  }
  (void)snprintf(
      fields, sizeof(fields),
      "activity=%lu lec=%lu dlec=%lu tec=%lu rec=%lu passive=%lu warning=%lu busoff=%lu restricted=%lu rxfill=%lu txfree=%lu warning_count=%lu passive_count=%lu busoff_count=%lu protocol_error_count=%lu rx_fifo_full_count=%lu rx_fifo_lost_count=%lu recovery_count=%lu recovery_failure_count=%lu",
      (unsigned long)diagnostics.activity,
      (unsigned long)diagnostics.last_error_code,
      (unsigned long)diagnostics.data_last_error_code,
      (unsigned long)diagnostics.tx_error_count,
      (unsigned long)diagnostics.rx_error_count,
      (unsigned long)diagnostics.error_passive,
      (unsigned long)diagnostics.warning,
      (unsigned long)diagnostics.bus_off,
      (unsigned long)diagnostics.restricted_mode,
      (unsigned long)diagnostics.rx_fifo_fill,
      (unsigned long)diagnostics.tx_fifo_free,
      (unsigned long)diagnostics.warning_count,
      (unsigned long)diagnostics.error_passive_count,
      (unsigned long)diagnostics.bus_off_count,
      (unsigned long)diagnostics.protocol_error_count,
      (unsigned long)diagnostics.rx_fifo_full_count,
      (unsigned long)diagnostics.rx_fifo_lost_count,
      (unsigned long)diagnostics.recovery_count,
      (unsigned long)diagnostics.recovery_failure_count);
  (void)UartProtocol_SendResponse(now_ms, "can_status", true, fields);
}

static void SendConsoleHelp(uint32_t now_ms)
{
  const char *commands =
      "commands=help,ping,status,telemetry,can_status,can_tx,pid_show,pid_set,"
      "pid_target,pid_stop,encoder_zero,encoder_result,odometry_reset,ota_uart,qspi_test,"
      "iwdg_reset_test,motor_duty,motor_stop,motor_left_forward,motor_left_reverse,"
      "motor_right_forward,motor_right_reverse";

  (void)UartProtocol_SendResponse(now_ms, "help", true, commands);
}

static const char *ParameterPersistenceText(
    const ParameterStorageSnapshot *snapshot)
{
  if (snapshot == NULL) {
    return "ERROR";
  }
  if (snapshot->dirty) {
    return snapshot->status == PARAMETER_STORAGE_SAVING ? "SAVING" : "QUEUED";
  }
  switch (snapshot->status) {
    case PARAMETER_STORAGE_LOADED:
    case PARAMETER_STORAGE_STORED:
      return "STORED";
    case PARAMETER_STORAGE_DEFAULTS:
      return "DEFAULTS";
    case PARAMETER_STORAGE_ERROR:
      return "ERROR";
    case PARAMETER_STORAGE_SAVING:
      return "SAVING";
    default:
      return "ERROR";
  }
}

static const char *MotionCommandErrorCode(
    CommandManagerSubmitResult submit_result)
{
  if (submit_result == COMMAND_SUBMIT_INVALID_ARGUMENT) {
    return "INVALID_ARGUMENT";
  }
  if (submit_result == COMMAND_SUBMIT_NOT_OWNER) {
    return "NOT_OWNER";
  }
  return SafetyManager_GetState() == CHASSIS_CONTROL_OPEN_LOOP_TEST
             ? "BUSY"
             : "SAFETY_STOP";
}
