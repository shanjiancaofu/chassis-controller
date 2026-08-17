#include "app/chassis_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "bsp/button/bsp_button.h"
#include "bsp/encoder/bsp_encoder.h"
#include "bsp/imu/bsp_icm45686.h"
#include "bsp/lcd/bsp_lcd.h"
#include "bsp/motor/bsp_motor.h"
#include "bsp/power_monitor/bsp_power_sample.h"
#include "bsp/sr501/bsp_sr501.h"
#include "bsp/uart/uart_bsp.h"
#include "communication/can_transport/can_transport.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "communication/ota_transport/ota_confirmation.h"
#include "communication/ota_transport/ota_session.h"
#include "communication/ota_transport/ota_uart_arm_guard.h"
#include "communication/ota_transport/ota_uart_transport.h"
#include "config/app_config.h"
#include "config/control_config.h"
#include "config/feature_config.h"
#include "iwdg.h"
#include "infrastructure/console/console.h"
#include "infrastructure/console/diagnostic_report.h"
#include "infrastructure/telemetry/telemetry.h"
#include "main.h"
#include "modules/chassis/command_manager.h"
#include "modules/chassis/odometry.h"
#include "modules/chassis/wheel_controller.h"
#include "modules/diagnostics/board_health.h"
#include "modules/parameters/parameter_manager.h"
#include "modules/safety/fault_manager.h"
#include "modules/safety/safety_manager.h"
#include "rtos/rtos_app.h"
#include "infrastructure/status_display/status_display.h"
#include "tests/target/iwdg_target_test.h"
#include "tests/target/motor_target_test.h"
#include "tests/target/qspi_target_test.h"
#include "task.h"

#define OTA_UART_ARM_TIMEOUT_MS 30000U

static uint32_t consecutive_control_overruns;
static OtaUartArmGuard ota_uart_arm_guard;
static bool ota_terminal_cleaned;
static bool ota_response_waiting;
static OtaResponse ota_response;

static void HandleConsoleCommand(const ConsoleCommand *command,
                                 uint32_t now_ms);
static void WritePidReport(bool accepted);
static void WriteEncoderResult(void);
static void WriteCanDiagnostics(void);
static void WriteConsoleHelp(void);
static bool StartControl(void);
static void StopControl(void);
static void ReleaseMotionOwner(void);
static bool StartMotorTargetTest(MotorTargetTestAction action,
                                 uint32_t now_ms);
static bool PrepareTargetTest(void);
static bool PrepareOta(void);
static void ReleaseOta(void);
static void RunOtaTransports(uint32_t now_ms);
static void ReleaseCompletedTargetTest(void);
static void LatchInternalFault(uint32_t fault);
static void ApplyPendingParameters(void);
static bool ResetOdometry(void);
static bool SubmitCommand(int32_t left_target, int32_t right_target,
                          CommandSource source, uint32_t now_ms,
                          bool has_sequence, uint8_t sequence);

#if ENABLE_MOTOR_DEMO
static uint8_t demo_stage;
static uint32_t demo_stage_started_ms;
#endif

void ChassisApp_Init(void)
{
  static const char startup_message[] = "chassis-controller started\r\n";

  if (!BspUart_Start()) {
    Error_Handler();
  }
  Console_Init();
  Telemetry_Init();
  if (!BspUart_Write(startup_message, sizeof(startup_message) - 1U)) {
    Error_Handler();
  }
  if (CanTransport_Init() != HAL_OK) {
    Error_Handler();
  }
  OtaCanTransport_Init();
  OtaUartTransport_Init();
  OtaSession_Init();
  OtaUartArmGuard_Init(&ota_uart_arm_guard);
  ota_terminal_cleaned = true;
  ota_response_waiting = false;

  BspMotor_Init();
  if (!BspMotor_Start()) {
    Error_Handler();
  }
  BspEncoder_Init();
  if (!BspEncoder_Start() || !BspPowerSample_Init()) {
    Error_Handler();
  }
  BspButton_Init();
  BspSr501_Init(HAL_GetTick());
#if ENABLE_ICM45686
  BspIcm45686_Init(HAL_GetTick());
#endif
  (void)StatusDisplay_Init();

  CommandManager_Init();
  FaultManager_Init();
  SafetyManager_Init(
      HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET);
  ParameterManager_Init();
  WheelController_Init();
  Odometry_Init();
  BoardHealth_Init();
  OtaConfirmation_Init();
  QspiTargetTest_Init();
  IwdgTargetTest_Init();
  MotorTargetTest_Init();
  DiagnosticReport_Init(HAL_GetTick());
  consecutive_control_overruns = 0U;
  if (SafetyManager_IsEmergencyStopLatched()) {
    WheelController_EmergencyStop();
  }
#if ENABLE_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = HAL_GetTick();
  if (!SubmitCommand(0, 0, COMMAND_SOURCE_TARGET_TEST,
                     demo_stage_started_ms,
                     false, 0U) ||
      !StartControl()) {
    Error_Handler();
  }
#endif
}

void ChassisApp_Run(void)
{
  static uint32_t last_heartbeat_ms;
  ConsoleCommand console_command;
  CanTransportControlCommand control_command;
  const uint32_t now_ms = HAL_GetTick();
  const CanTransportLinkStatus can_status = CanTransport_GetLinkStatus();

  BspButton_Run(now_ms);
  BspSr501_Run(now_ms);
#if ENABLE_ICM45686
  BspIcm45686_Run(now_ms);
#endif
  BspUart_Run();
  if (OtaUartTransport_IsEnabled()) {
    OtaUartTransport_Run();
  } else {
    Console_Run();
  }
  while (Console_TakeCommand(&console_command)) {
    HandleConsoleCommand(&console_command, now_ms);
  }

#if ENABLE_MOTOR_DEMO
  if (demo_stage < 6U) {
    taskENTER_CRITICAL();
    (void)CommandManager_Refresh(COMMAND_SOURCE_TARGET_TEST, now_ms);
    taskEXIT_CRITICAL();
  }
  switch (demo_stage) {
    case 0U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitCommand(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            COMMAND_SOURCE_TARGET_TEST, now_ms, false, 0U);
        demo_stage = 1U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 1U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)SubmitCommand(0, 0, COMMAND_SOURCE_TARGET_TEST, now_ms,
                            false, 0U);
        demo_stage = 2U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 2U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitCommand(-MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            COMMAND_SOURCE_TARGET_TEST, now_ms, false, 0U);
        demo_stage = 3U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 3U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)SubmitCommand(0, 0, COMMAND_SOURCE_TARGET_TEST, now_ms,
                            false, 0U);
        demo_stage = 4U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 4U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitCommand(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            COMMAND_SOURCE_TARGET_TEST, now_ms, false, 0U);
        demo_stage = 5U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 5U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        StopControl();
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        demo_stage = 6U;
      }
      break;
    default:
      break;
  }
#endif

  CanTransport_Run();
  if (CanTransport_TakeSessionInvalidated()) {
    OtaCanTransport_Invalidate();
    OtaSession_AbortSource(OTA_SOURCE_CAN_FD, now_ms);
    taskENTER_CRITICAL();
    if (CommandManager_GetOwner() == COMMAND_SOURCE_CAN_REMOTE) {
      taskEXIT_CRITICAL();
      StopControl();
      taskENTER_CRITICAL();
      CommandManager_Release(COMMAND_SOURCE_CAN_REMOTE);
      taskEXIT_CRITICAL();
    } else {
      taskEXIT_CRITICAL();
    }
  }
  RunOtaTransports(now_ms);
  if (CanTransport_TakeControlCommand(&control_command)) {
    if (control_command.enabled) {
      if (SubmitCommand(control_command.left_target,
                        control_command.right_target,
                        COMMAND_SOURCE_CAN_REMOTE,
                        now_ms, true, control_command.sequence)) {
        (void)StartControl();
      }
    } else if (CommandManager_GetOwner() ==
               COMMAND_SOURCE_CAN_REMOTE) {
      StopControl();
      taskENTER_CRITICAL();
      CommandManager_Release(COMMAND_SOURCE_CAN_REMOTE);
      taskEXIT_CRITICAL();
    }
  }

  StatusDisplay_Run(now_ms);
  QspiTargetTest_Run(now_ms);
  {
    BoardHealthSnapshot health;

    BoardHealth_GetSnapshot(&health);
    OtaConfirmation_Run(
        now_ms,
        health.qspi_id_valid && RtosApp_AreCriticalTasksHealthy() &&
            !FaultManager_HasCritical(),
        QspiTargetTest_GetStatus() != QSPI_TARGET_TEST_RUNNING &&
            !OtaSession_IsUsingQspi());
  }
  if (OtaSession_HasCriticalFault() ||
      OtaConfirmation_HasCriticalFault() ||
      QspiTargetTest_HasCriticalFault()) {
    LatchInternalFault(CHASSIS_FAULT_INTERNAL);
  }
  MotorTargetTest_Run(now_ms);
  ReleaseCompletedTargetTest();
  DiagnosticReport_Run(now_ms);

  if (IwdgTargetTest_IsResetRequested()) {
    StopControl();
    BspMotor_EmergencyStop();
  }

  if (OtaSession_IsResetRequested(now_ms) &&
      ((OtaSession_GetSource() == OTA_SOURCE_UART &&
        OtaUartTransport_IsTxIdle()) ||
       (OtaSession_GetSource() == OTA_SOURCE_CAN_FD &&
        OtaCanTransport_IsTxIdle()))) {
    StopControl();
    BspMotor_CoastAll();
    if (AppWatchdog_PrepareForBootloader()) {
      NVIC_SystemReset();
    }
  }

  if (Telemetry_IsDue(now_ms)) {
    WheelControllerSnapshot wheel_snapshot;
    MotorTargetTestSnapshot motor_test_snapshot;
    OdometrySnapshot odometry_snapshot;
    TelemetrySnapshot snapshot;
    uint32_t supply_mv;
    const bool supply_valid =
        BspPowerSample_ReadMillivolts(&supply_mv);

    taskENTER_CRITICAL();
    WheelController_GetSnapshot(&wheel_snapshot);
    MotorTargetTest_GetSnapshot(&motor_test_snapshot);
    Odometry_GetSnapshot(&odometry_snapshot);
    taskEXIT_CRITICAL();
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
    snapshot.supply_mv = supply_valid ? (int32_t)supply_mv : -1;
    snapshot.control_state = (uint32_t)SafetyManager_GetState();
    snapshot.fault_flags = FaultManager_GetFlags();
    Telemetry_Run(now_ms, &snapshot);
  }

  if (now_ms - last_heartbeat_ms >= 500U) {
    last_heartbeat_ms = now_ms;
    HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
  }
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
  if (can_status == CAN_TRANSPORT_LINK_PASSED) {
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  } else if (can_status == CAN_TRANSPORT_LINK_FAILED) {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
  }

  if (RtosApp_AreCriticalTasksHealthy() &&
      !FaultManager_HasCritical() &&
      CanTransport_IsOperational() &&
      !IwdgTargetTest_IsResetRequested()) {
    HAL_IWDG_Refresh(&hiwdg);
  }
}

static void HandleConsoleCommand(const ConsoleCommand *command,
                                 uint32_t now_ms)
{
  static const char pong[] = "PONG\r\n";
  static const char telemetry_text[] = "TELEMETRY: TEXT\r\n";
  static const char telemetry_vofa[] = "TELEMETRY: VOFA\r\n";
  static const char telemetry_off[] = "TELEMETRY: OFF\r\n";
  static const char can_queued[] = "FDCAN_DIAG: TX_721_QUEUED\r\n";
  char response[96];

  if (command == NULL) {
    return;
  }

  switch (command->type) {
    case CONSOLE_COMMAND_PING:
      (void)BspUart_WriteString(pong);
      break;
    case CONSOLE_COMMAND_STATUS:
      DiagnosticReport_RequestSelfTest();
      break;
    case CONSOLE_COMMAND_TELEMETRY_TEXT:
      Telemetry_SetMode(TELEMETRY_MODE_TEXT);
      (void)BspUart_WriteString(telemetry_text);
      break;
    case CONSOLE_COMMAND_TELEMETRY_VOFA:
      Telemetry_SetMode(TELEMETRY_MODE_VOFA);
      (void)BspUart_WriteString(telemetry_vofa);
      break;
    case CONSOLE_COMMAND_TELEMETRY_OFF:
      Telemetry_SetMode(TELEMETRY_MODE_OFF);
      (void)BspUart_WriteString(telemetry_off);
      break;
    case CONSOLE_COMMAND_CAN_STATUS:
      WriteCanDiagnostics();
      break;
    case CONSOLE_COMMAND_CAN_TRANSMIT:
      CanTransport_RequestResponse();
      (void)BspUart_WriteString(can_queued);
      break;
    case CONSOLE_COMMAND_PID_SHOW:
      WritePidReport(true);
      break;
    case CONSOLE_COMMAND_PID_SET_LEFT:
    case CONSOLE_COMMAND_PID_SET_RIGHT: {
      bool accepted;

      taskENTER_CRITICAL();
      accepted = ParameterManager_StagePidGains(
          command->type == CONSOLE_COMMAND_PID_SET_LEFT
              ? PARAMETER_WHEEL_LEFT
              : PARAMETER_WHEEL_RIGHT,
          command->arguments.pid.kp, command->arguments.pid.ki,
          command->arguments.pid.kd);
      taskEXIT_CRITICAL();
      WritePidReport(accepted);
      break;
    }
    case CONSOLE_COMMAND_PID_TARGET:
      if (SubmitCommand(command->arguments.target.left,
                        command->arguments.target.right,
                        COMMAND_SOURCE_CONSOLE, now_ms, false, 0U)) {
        (void)StartControl();
      }
      break;
    case CONSOLE_COMMAND_PID_STOP:
      StopControl();
      taskENTER_CRITICAL();
      CommandManager_Release(COMMAND_SOURCE_CONSOLE);
      taskEXIT_CRITICAL();
      break;
    case CONSOLE_COMMAND_ENCODER_ZERO:
      (void)snprintf(response, sizeof(response), "ENCODER_CAL: %s\r\n",
                     ResetOdometry()
                         ? "RESET"
                         : "REJECTED, stop motor first");
      (void)BspUart_WriteString(response);
      break;
    case CONSOLE_COMMAND_ENCODER_RESULT:
      WriteEncoderResult();
      break;
    case CONSOLE_COMMAND_OTA_UART:
      if (PrepareOta()) {
        static const char ready[] =
            "OTA_UART: READY, binary mode\r\n";

        Telemetry_SetMode(TELEMETRY_MODE_OFF);
        (void)BspUart_WriteString(ready);
        OtaUartTransport_Enable();
        OtaUartArmGuard_Arm(&ota_uart_arm_guard, now_ms);
        ota_terminal_cleaned = false;
      } else {
        (void)BspUart_WriteString("OTA_UART: REJECTED\r\n");
      }
      break;
    case CONSOLE_COMMAND_QSPI_TEST:
      if (!PrepareTargetTest() ||
          !QspiTargetTest_Start()) {
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        WriteConsoleHelp();
      } else {
        DiagnosticReport_RequestQspiTest();
      }
      break;
    case CONSOLE_COMMAND_IWDG_RESET:
      if (!PrepareTargetTest() ||
          !IwdgTargetTest_Start()) {
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        WriteConsoleHelp();
      } else {
        DiagnosticReport_RequestIwdgArmed();
      }
      break;
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
        StopControl();
        taskENTER_CRITICAL();
        CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
        taskEXIT_CRITICAL();
        accepted = true;
      } else {
        accepted = PrepareTargetTest() &&
                   StartMotorTargetTest(action, now_ms);
        if (!accepted) {
          taskENTER_CRITICAL();
          CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
          taskEXIT_CRITICAL();
        }
      }
      DiagnosticReport_MotorTestResult(action, accepted);
      break;
    }
    case CONSOLE_COMMAND_HELP:
      WriteConsoleHelp();
      break;
    default:
      break;
  }
}

static void WritePidReport(bool accepted)
{
  char response[160];
  int length;
  ParameterSnapshot parameters;

  taskENTER_CRITICAL();
  ParameterManager_GetRequested(&parameters);
  taskEXIT_CRITICAL();
  if (accepted) {
    length = snprintf(
        response, sizeof(response),
        "PID: left kp=%u ki=%u kd=%u right kp=%u ki=%u kd=%u\r\n",
        (unsigned int)parameters.left_pid.kp,
        (unsigned int)parameters.left_pid.ki,
        (unsigned int)parameters.left_pid.kd,
        (unsigned int)parameters.right_pid.kp,
        (unsigned int)parameters.right_pid.ki,
        (unsigned int)parameters.right_pid.kd);
  } else {
    length = snprintf(
        response, sizeof(response),
        "PID: REJECTED limits kp<=%u ki<=%u kd<=%u\r\n",
        (unsigned int)MOTOR_PID_KP_MAX, (unsigned int)MOTOR_PID_KI_MAX,
        (unsigned int)MOTOR_PID_KD_MAX);
  }
  if (length > 0 && (size_t)length < sizeof(response)) {
    (void)BspUart_Write(response, (size_t)length);
  }
}

static void WriteEncoderResult(void)
{
  OdometrySnapshot snapshot;
  char response[96];
  int length;

  taskENTER_CRITICAL();
  Odometry_GetSnapshot(&snapshot);
  taskEXIT_CRITICAL();
  length = snprintf(response, sizeof(response),
                    "ENCODER_CAL: left=%lld right=%lld counts\r\n",
                    (long long)snapshot.left_total,
                    (long long)snapshot.right_total);
  if (length > 0 && (size_t)length < sizeof(response)) {
    (void)BspUart_Write(response, (size_t)length);
  }
}

static void WriteCanDiagnostics(void)
{
  CanTransportDiagnostics diagnostics;
  char response[320];
  int length;

  if (!CanTransport_GetDiagnostics(&diagnostics)) {
    (void)BspUart_WriteString("FDCAN_DIAG: READ_FAILED\r\n");
    return;
  }
  length = snprintf(
      response, sizeof(response),
      "FDCAN_DIAG: activity=%lu lec=%lu dlec=%lu tec=%lu rec=%lu passive=%lu warning=%lu busoff=%lu restricted=%lu rxfill=%lu txfree=%lu counts[w=%lu p=%lu bo=%lu pe=%lu full=%lu lost=%lu recover=%lu fail=%lu]\r\n",
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
  if (length > 0 && (size_t)length < sizeof(response)) {
    (void)BspUart_Write(response, (size_t)length);
  }
}

static void WriteConsoleHelp(void)
{
  size_t length;
  const char *text = Console_GetHelpText(&length);

  (void)BspUart_Write(text, length);
}

static bool StartControl(void)
{
  CommandManagerCommand command;
  bool accepted;
  uint32_t primask;

  primask = __get_PRIMASK();
  __disable_irq();
  accepted = CommandManager_Get(&command) &&
             SafetyManager_RequestRun(true);
  __set_PRIMASK(primask);
  return accepted;
}

static void StopControl(void)
{
  taskENTER_CRITICAL();
  CommandManager_ClearCommand();
  MotorTargetTest_Stop();
  WheelController_Stop();
  SafetyManager_Stop();
  taskEXIT_CRITICAL();
}

static void ReleaseMotionOwner(void)
{
  const CommandSource owner = CommandManager_GetOwner();

  if (owner == COMMAND_SOURCE_CAN_REMOTE ||
      owner == COMMAND_SOURCE_CONSOLE ||
      owner == COMMAND_SOURCE_TARGET_TEST) {
    CommandManager_Release(owner);
  }
}

void ChassisApp_RunControlTick(uint32_t notification_count)
{
  CommandManagerCommand command;
  int32_t left_delta;
  int32_t left_measurement;
  int32_t right_delta;
  int32_t right_measurement;
  bool command_available;
  uint32_t missed_ticks;
  const uint32_t now_ms = HAL_GetTick();

  if (notification_count == 0U) {
    return;
  }

  missed_ticks = notification_count - 1U;
  if (missed_ticks > 0U) {
    BoardHealth_RecordControlOverrun(missed_ticks);
    ++consecutive_control_overruns;
    if (missed_ticks > MOTOR_CONTROL_MAX_MISSED_TICKS ||
        consecutive_control_overruns >=
            MOTOR_CONTROL_MAX_CONSECUTIVE_OVERRUNS) {
      LatchInternalFault(CHASSIS_FAULT_CONTROL_OVERRUN);
      return;
    }
  } else {
    consecutive_control_overruns = 0U;
  }

  BspEncoder_ReadDelta(&left_delta, &right_delta);
  Odometry_Update(left_delta, right_delta);
  left_measurement = left_delta / (int32_t)notification_count;
  right_measurement = right_delta / (int32_t)notification_count;
  ApplyPendingParameters();
  if (SafetyManager_IsEmergencyStopLatched()) {
    taskENTER_CRITICAL();
    CommandManager_ClearCommand();
    ReleaseMotionOwner();
    taskEXIT_CRITICAL();
    WheelController_EmergencyStop();
    return;
  }

  if (FaultManager_HasCritical()) {
    taskENTER_CRITICAL();
    CommandManager_ClearCommand();
    ReleaseMotionOwner();
    taskEXIT_CRITICAL();
    WheelController_EmergencyStop();
    SafetyManager_LatchInternalFault();
    return;
  }

  if (SafetyManager_IsOpenLoopTestRunning()) {
    WheelController_Reset();
    return;
  }

  if (!SafetyManager_IsRunning()) {
    WheelController_Stop();
    return;
  }

  taskENTER_CRITICAL();
  command_available = !CommandManager_IsTimedOut(now_ms) &&
                      CommandManager_Get(&command);
  if (!command_available) {
    CommandManager_ClearCommand();
    ReleaseMotionOwner();
  }
  taskEXIT_CRITICAL();
  if (!command_available) {
    WheelController_Stop();
    SafetyManager_EnterCommandTimeout();
    return;
  }

  if (!WheelController_Update(command.left_target, command.right_target,
                              left_measurement, right_measurement)) {
    LatchInternalFault(CHASSIS_FAULT_INTERNAL);
  }
}

static bool StartMotorTargetTest(MotorTargetTestAction action,
                                 uint32_t now_ms)
{
  if (!SafetyManager_RequestOpenLoopTest()) {
    return false;
  }
  taskENTER_CRITICAL();
  WheelController_Stop();
  taskEXIT_CRITICAL();
  if (!MotorTargetTest_Start(action, now_ms)) {
    SafetyManager_Stop();
    return false;
  }
  return true;
}

static bool PrepareTargetTest(void)
{
  MotorTargetTestSnapshot motor_test;

  MotorTargetTest_GetSnapshot(&motor_test);
  if (IwdgTargetTest_IsResetRequested() ||
      QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING ||
      OtaConfirmation_IsUsingQspi() ||
      OtaSession_IsActive() || OtaUartTransport_IsEnabled() ||
      motor_test.running) {
    return false;
  }

  StopControl();
  BspMotor_CoastAll();
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED ||
      BspMotor_GetAppliedDuty(BSP_MOTOR_LEFT) != 0 ||
      BspMotor_GetAppliedDuty(BSP_MOTOR_RIGHT) != 0) {
    return false;
  }

  taskENTER_CRITICAL();
  ReleaseMotionOwner();
  const bool acquired =
      CommandManager_Acquire(COMMAND_SOURCE_TARGET_TEST);
  taskEXIT_CRITICAL();
  return acquired;
}

static bool PrepareOta(void)
{
  MotorTargetTestSnapshot motor_test;

  MotorTargetTest_GetSnapshot(&motor_test);
  if (IwdgTargetTest_IsResetRequested() ||
      QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING ||
      OtaConfirmation_IsUsingQspi() || OtaSession_IsActive() ||
      motor_test.running) {
    return false;
  }

  StopControl();
  BspMotor_CoastAll();
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED ||
      BspMotor_GetAppliedDuty(BSP_MOTOR_LEFT) != 0 ||
      BspMotor_GetAppliedDuty(BSP_MOTOR_RIGHT) != 0) {
    return false;
  }

  taskENTER_CRITICAL();
  ReleaseMotionOwner();
  const bool acquired = CommandManager_Acquire(COMMAND_SOURCE_OTA);
  taskEXIT_CRITICAL();
  if (acquired) {
    ota_terminal_cleaned = false;
  }
  return acquired;
}

static void ReleaseOta(void)
{
  taskENTER_CRITICAL();
  CommandManager_Release(COMMAND_SOURCE_OTA);
  taskEXIT_CRITICAL();
}

static void RunOtaTransports(uint32_t now_ms)
{
  OtaMessage message;
  bool begin_allowed;
  bool prepared_here;
  bool response_submitted;

  if (!ota_response_waiting &&
      (OtaCanTransport_TakeMessage(&message) ||
       OtaUartTransport_TakeMessage(&message))) {
    prepared_here = false;
    if (message.type == OTA_MESSAGE_BEGIN && !OtaSession_IsActive() &&
        CommandManager_GetOwner() != COMMAND_SOURCE_OTA) {
      prepared_here = PrepareOta();
    }
    begin_allowed = CommandManager_GetOwner() == COMMAND_SOURCE_OTA;
    if (message.type == OTA_MESSAGE_BEGIN &&
        OtaUartTransport_IsEnabled() &&
        message.source != OTA_SOURCE_UART) {
      begin_allowed = false;
    }
    (void)OtaSession_Submit(&message, now_ms, begin_allowed);
    if (message.type == OTA_MESSAGE_BEGIN &&
        message.source == OTA_SOURCE_UART && OtaSession_IsActive() &&
        OtaSession_GetSource() == OTA_SOURCE_UART) {
      OtaUartArmGuard_EndWait(&ota_uart_arm_guard);
    }
    if (prepared_here && !OtaSession_IsActive()) {
      ReleaseOta();
    }
  }

  OtaSession_Run(now_ms);
  if (!ota_response_waiting) {
    ota_response_waiting = OtaSession_TakeResponse(&ota_response);
  }
  if (ota_response_waiting) {
    response_submitted =
        ota_response.source == OTA_SOURCE_CAN_FD
            ? OtaCanTransport_SendResponse(&ota_response)
            : ota_response.source == OTA_SOURCE_UART
                  ? OtaUartTransport_SendResponse(&ota_response)
                  : false;
    if (response_submitted) {
      ota_response_waiting = false;
      OtaSession_ResponseSubmitted();
      if (ota_response.source == OTA_SOURCE_CAN_FD) {
        OtaCanTransport_ResponseAccepted();
      }
    }
  }

  if (!ota_terminal_cleaned && !ota_response_waiting &&
      (OtaSession_GetState() == OTA_TRANSFER_ABORTED ||
       OtaSession_GetState() == OTA_TRANSFER_FAILED) &&
      !OtaSession_IsActive()) {
    if (OtaSession_GetSource() == OTA_SOURCE_UART) {
      OtaUartTransport_Disable();
      OtaUartArmGuard_EndWait(&ota_uart_arm_guard);
    }
    ReleaseOta();
    ota_terminal_cleaned = true;
  }

  if (OtaUartTransport_IsEnabled() &&
      OtaUartArmGuard_ShouldTimeout(
          &ota_uart_arm_guard, now_ms, OTA_UART_ARM_TIMEOUT_MS,
          OtaSession_IsActive(), ota_response_waiting)) {
    OtaUartTransport_Disable();
    OtaUartArmGuard_EndWait(&ota_uart_arm_guard);
    ReleaseOta();
    ota_terminal_cleaned = true;
    (void)BspUart_WriteString("OTA_UART: TIMEOUT, text mode\r\n");
  }
}

static void ReleaseCompletedTargetTest(void)
{
  MotorTargetTestSnapshot motor_test;

  if (IwdgTargetTest_IsResetRequested() ||
      QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING) {
    return;
  }
  MotorTargetTest_GetSnapshot(&motor_test);
  if (motor_test.running) {
    return;
  }

  taskENTER_CRITICAL();
  CommandManager_Release(COMMAND_SOURCE_TARGET_TEST);
  taskEXIT_CRITICAL();
}

static void LatchInternalFault(uint32_t fault)
{
  uint32_t primask;

  FaultManager_Raise(fault);
  SafetyManager_LatchInternalFault();
  primask = __get_PRIMASK();
  __disable_irq();
  CommandManager_ClearCommand();
  ReleaseMotionOwner();
  WheelController_EmergencyStop();
  __set_PRIMASK(primask);
}

static void ApplyPendingParameters(void)
{
  ParameterSnapshot parameters;

  taskENTER_CRITICAL();
  if (!ParameterManager_ApplyPending(&parameters)) {
    taskEXIT_CRITICAL();
    return;
  }
  taskEXIT_CRITICAL();

  WheelController_ApplyPidGains(
      WHEEL_CONTROLLER_LEFT, parameters.left_pid.kp,
      parameters.left_pid.ki, parameters.left_pid.kd);
  WheelController_ApplyPidGains(
      WHEEL_CONTROLLER_RIGHT, parameters.right_pid.kp,
      parameters.right_pid.ki, parameters.right_pid.kd);
}

static bool ResetOdometry(void)
{
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED) {
    return false;
  }

  taskENTER_CRITICAL();
  Odometry_Reset();
  taskEXIT_CRITICAL();
  return true;
}

static bool SubmitCommand(int32_t left_target, int32_t right_target,
                          CommandSource source, uint32_t now_ms,
                          bool has_sequence, uint8_t sequence)
{
  bool accepted;
  uint32_t primask;
  const CommandManagerCommand command = {
      .left_target = left_target,
      .right_target = right_target,
      .received_ms = now_ms,
      .source = source,
      .sequence = sequence,
      .has_sequence = has_sequence,
  };

  primask = __get_PRIMASK();
  __disable_irq();
  accepted = CommandManager_Submit(&command);
  __set_PRIMASK(primask);
  return accepted;
}

void ChassisApp_FatalError(void)
{
  LatchInternalFault(CHASSIS_FAULT_INTERNAL);
}

bool ChassisApp_ClearEmergencyStop(void)
{
  uint32_t primask;

  if (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    __set_PRIMASK(primask);
    return false;
  }
  (void)SafetyManager_ClearEmergencyStop();
  BspMotor_ClearEmergencyStop();
  __set_PRIMASK(primask);

  StopControl();
  return true;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == BUTTON_1_Pin || gpio_pin == BUTTON_2_Pin) {
    BspButton_OnInterrupt(gpio_pin);
  }
  if (gpio_pin == IMU_INT1_Pin) {
    BspIcm45686_OnDataReadyInterrupt();
  }
  if (gpio_pin == KEY_Pin) {
    StatusDisplay_OnKeyInterrupt();
  }
  if (gpio_pin == E_STOP_Pin &&
      HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    BspMotor_EmergencyStop();
    SafetyManager_LatchEmergencyStopFromIsr();
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI2) {
    BspLcd_OnSpiTxComplete();
  }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI3) {
    BspIcm45686_OnSpiTransferComplete();
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == NULL) {
    return;
  }
  if (hspi->Instance == SPI2) {
    BspLcd_OnSpiError();
  } else if (hspi->Instance == SPI3) {
    BspIcm45686_OnSpiTransferError();
  }
}
