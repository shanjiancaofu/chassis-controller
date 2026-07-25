#include "app/chassis_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bsp/encoder/bsp_encoder.h"
#include "bsp/motor/bsp_motor.h"
#include "bsp/power_monitor/bsp_power_sample.h"
#include "bsp/uart/uart_bsp.h"
#include "communication/can_transport/can_transport.h"
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
#include "infrastructure/status_display/status_display.h"
#include "tests/target/iwdg_target_test.h"
#include "tests/target/motor_target_test.h"
#include "tests/target/qspi_target_test.h"
#include "tim.h"

static volatile uint8_t control_ticks_pending;
static volatile bool control_tick_overflow;

static void HandleConsoleCommand(const ConsoleCommand *command,
                                 uint32_t now_ms);
static void WritePidReport(bool accepted);
static void WriteEncoderResult(void);
static void WriteCanDiagnostics(void);
static void WriteConsoleHelp(void);
static bool StartControl(void);
static void StopControl(void);
static void RunControlTick(uint32_t now_ms);
static bool StartMotorTargetTest(MotorTargetTestAction action,
                                 uint32_t now_ms);
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

  BspMotor_Init();
  if (!BspMotor_Start()) {
    Error_Handler();
  }
  BspEncoder_Init();
  if (!BspEncoder_Start() || !BspPowerSample_Init()) {
    Error_Handler();
  }
  (void)StatusDisplay_Init();

  CommandManager_Init();
  FaultManager_Init();
  SafetyManager_Init(
      HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET);
  ParameterManager_Init();
  WheelController_Init();
  Odometry_Init();
  BoardHealth_Init();
  QspiTargetTest_Init();
  IwdgTargetTest_Init();
  MotorTargetTest_Init();
  DiagnosticReport_Init(HAL_GetTick());
  ApplyPendingParameters();
  if (SafetyManager_IsEmergencyStopLatched()) {
    WheelController_EmergencyStop();
  }
  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
    Error_Handler();
  }

#if ENABLE_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = HAL_GetTick();
  if (!SubmitCommand(0, 0, COMMAND_SOURCE_DEMO, demo_stage_started_ms,
                     false, 0U) ||
      !StartControl()) {
    Error_Handler();
  }
#endif
}

void ChassisApp_Run(void)
{
  static uint32_t last_heartbeat_ms;
  static uint32_t last_control_run_ms;
  ConsoleCommand console_command;
  uint8_t pending_ticks;
  bool tick_overflow;
  CanTransportControlCommand control_command;
  uint32_t primask;
  const uint32_t now_ms = HAL_GetTick();
  const CanTransportLinkStatus can_status = CanTransport_GetLinkStatus();

  BspUart_Run();
  Console_Run();
  while (Console_TakeCommand(&console_command)) {
    HandleConsoleCommand(&console_command, now_ms);
  }

#if ENABLE_MOTOR_DEMO
  if (demo_stage < 6U) {
    (void)CommandManager_Refresh(COMMAND_SOURCE_DEMO, now_ms);
  }
  switch (demo_stage) {
    case 0U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitCommand(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            COMMAND_SOURCE_DEMO, now_ms, false, 0U);
        demo_stage = 1U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 1U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)SubmitCommand(0, 0, COMMAND_SOURCE_DEMO, now_ms, false, 0U);
        demo_stage = 2U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 2U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitCommand(-MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            COMMAND_SOURCE_DEMO, now_ms, false, 0U);
        demo_stage = 3U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 3U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)SubmitCommand(0, 0, COMMAND_SOURCE_DEMO, now_ms, false, 0U);
        demo_stage = 4U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 4U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitCommand(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                            COMMAND_SOURCE_DEMO, now_ms, false, 0U);
        demo_stage = 5U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 5U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        StopControl();
        demo_stage = 6U;
      }
      break;
    default:
      break;
  }
#endif

  if (CanTransport_TakeControlCommand(&control_command)) {
    if (control_command.enabled) {
      if (SubmitCommand(control_command.left_target,
                        control_command.right_target, COMMAND_SOURCE_CAN,
                        now_ms, true, control_command.sequence)) {
        (void)StartControl();
      }
    } else {
      StopControl();
    }
  }

  primask = __get_PRIMASK();
  __disable_irq();
  pending_ticks = control_ticks_pending;
  control_ticks_pending = 0U;
  tick_overflow = control_tick_overflow;
  control_tick_overflow = false;
  __set_PRIMASK(primask);

  if (tick_overflow ||
      pending_ticks > MOTOR_CONTROL_MAX_PENDING_TICKS) {
    LatchInternalFault(CHASSIS_FAULT_CONTROL_OVERRUN);
  } else {
    while (pending_ticks > 0U) {
      RunControlTick(now_ms);
      --pending_ticks;
      last_control_run_ms = now_ms;
    }
  }

  StatusDisplay_Run(now_ms);
  CanTransport_Run();
  QspiTargetTest_Run(now_ms);
  MotorTargetTest_Run(now_ms);
  DiagnosticReport_Run(now_ms);

  if (IwdgTargetTest_IsResetRequested()) {
    StopControl();
    BspMotor_EmergencyStop();
  }

  if (Telemetry_IsDue(now_ms)) {
    WheelControllerSnapshot wheel_snapshot;
    MotorTargetTestSnapshot motor_test_snapshot;
    OdometrySnapshot odometry_snapshot;
    TelemetrySnapshot snapshot;
    uint32_t supply_mv;
    const bool supply_valid =
        BspPowerSample_ReadMillivolts(&supply_mv);

    WheelController_GetSnapshot(&wheel_snapshot);
    MotorTargetTest_GetSnapshot(&motor_test_snapshot);
    Odometry_GetSnapshot(&odometry_snapshot);
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
  if (can_status == CAN_TRANSPORT_LINK_PASSED) {
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  } else if (can_status == CAN_TRANSPORT_LINK_FAILED) {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
  }

  if (last_control_run_ms != 0U &&
      now_ms - last_control_run_ms <= 50U &&
      !FaultManager_HasCritical() &&
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
    case CONSOLE_COMMAND_PID_SET_RIGHT:
      WritePidReport(ParameterManager_StagePidGains(
          command->type == CONSOLE_COMMAND_PID_SET_LEFT
              ? PARAMETER_WHEEL_LEFT
              : PARAMETER_WHEEL_RIGHT,
          command->arguments.pid.kp, command->arguments.pid.ki,
          command->arguments.pid.kd));
      break;
    case CONSOLE_COMMAND_PID_TARGET:
      if (SubmitCommand(command->arguments.target.left,
                        command->arguments.target.right,
                        COMMAND_SOURCE_CONSOLE, now_ms, false, 0U)) {
        (void)StartControl();
      }
      break;
    case CONSOLE_COMMAND_PID_STOP:
      StopControl();
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
    case CONSOLE_COMMAND_QSPI_TEST:
      if (IwdgTargetTest_IsResetRequested() ||
          !QspiTargetTest_Start()) {
        WriteConsoleHelp();
      } else {
        DiagnosticReport_RequestQspiTest();
      }
      break;
    case CONSOLE_COMMAND_IWDG_RESET:
      if (QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING ||
          !IwdgTargetTest_Start()) {
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
        accepted = true;
      } else {
        accepted = !IwdgTargetTest_IsResetRequested() &&
                   QspiTargetTest_GetStatus() != QSPI_TARGET_TEST_RUNNING &&
                   StartMotorTargetTest(action, now_ms);
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

  ParameterManager_GetRequested(&parameters);
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

  Odometry_GetSnapshot(&snapshot);
  length = snprintf(response, sizeof(response),
                    "ENCODER_CAL: left=%ld right=%ld counts\r\n",
                    (long)snapshot.left_total, (long)snapshot.right_total);
  if (length > 0 && (size_t)length < sizeof(response)) {
    (void)BspUart_Write(response, (size_t)length);
  }
}

static void WriteCanDiagnostics(void)
{
  CanTransportDiagnostics diagnostics;
  char response[256];
  int length;

  if (!CanTransport_GetDiagnostics(&diagnostics)) {
    (void)BspUart_WriteString("FDCAN_DIAG: READ_FAILED\r\n");
    return;
  }
  length = snprintf(
      response, sizeof(response),
      "FDCAN_DIAG: activity=%lu lec=%lu dlec=%lu tec=%lu rec=%lu passive=%lu warning=%lu busoff=%lu restricted=%lu rxfill=%lu txfree=%lu\r\n",
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
      (unsigned long)diagnostics.tx_fifo_free);
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

  return CommandManager_Get(&command) &&
         SafetyManager_RequestRun(true);
}

static void StopControl(void)
{
  CommandManager_Clear();
  MotorTargetTest_Stop();
  WheelController_Stop();
  SafetyManager_Stop();
}

static void RunControlTick(uint32_t now_ms)
{
  CommandManagerCommand command;
  OdometrySnapshot odometry;
  int32_t left_delta;
  int32_t right_delta;

  BspEncoder_ReadDelta(&left_delta, &right_delta);
  Odometry_Update(left_delta, right_delta);
  ApplyPendingParameters();

  if (SafetyManager_IsEmergencyStopLatched()) {
    CommandManager_Clear();
    WheelController_EmergencyStop();
    return;
  }

  if (FaultManager_HasCritical()) {
    CommandManager_Clear();
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

  if (CommandManager_IsTimedOut(now_ms) ||
      !CommandManager_Get(&command)) {
    CommandManager_Clear();
    WheelController_Stop();
    SafetyManager_EnterCommandTimeout();
    return;
  }

  Odometry_GetSnapshot(&odometry);
  if (!WheelController_Update(command.left_target, command.right_target,
                              odometry.left_delta,
                              odometry.right_delta)) {
    LatchInternalFault(CHASSIS_FAULT_INTERNAL);
  }
}

static bool StartMotorTargetTest(MotorTargetTestAction action,
                                 uint32_t now_ms)
{
  if (!SafetyManager_RequestOpenLoopTest()) {
    return false;
  }
  CommandManager_Clear();
  WheelController_Stop();
  if (!MotorTargetTest_Start(action, now_ms)) {
    SafetyManager_Stop();
    return false;
  }
  return true;
}

static void LatchInternalFault(uint32_t fault)
{
  FaultManager_Raise(fault);
  SafetyManager_LatchInternalFault();
  CommandManager_Clear();
  WheelController_EmergencyStop();
}

static void ApplyPendingParameters(void)
{
  ParameterSnapshot parameters;

  if (!ParameterManager_ApplyPending(&parameters)) {
    return;
  }

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

  Odometry_Reset();
  return true;
}

static bool SubmitCommand(int32_t left_target, int32_t right_target,
                          CommandSource source, uint32_t now_ms,
                          bool has_sequence, uint8_t sequence)
{
  const CommandManagerCommand command = {
      .left_target = left_target,
      .right_target = right_target,
      .received_ms = now_ms,
      .source = source,
      .sequence = sequence,
      .has_sequence = has_sequence,
  };

  return CommandManager_Submit(&command);
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

void ChassisApp_ControlTickFromIsr(void)
{
  if (control_ticks_pending < UINT8_MAX) {
    ++control_ticks_pending;
  } else {
    control_tick_overflow = true;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == KEY_Pin) {
    StatusDisplay_OnKeyInterrupt();
  }
  if (gpio_pin == E_STOP_Pin &&
      HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    BspMotor_EmergencyStop();
    SafetyManager_LatchEmergencyStopFromIsr();
  }
}
