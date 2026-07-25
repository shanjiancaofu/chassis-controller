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
#include "infrastructure/telemetry/telemetry.h"
#include "main.h"
#include "modules/chassis_control/chassis_control.h"
#include "modules/command_manager/command_manager.h"
#include "modules/diagnostics/board_self_test.h"
#include "infrastructure/status_display/status_display.h"
#include "tim.h"

static volatile uint8_t control_ticks_pending;
static volatile bool control_tick_overflow;

static void HandleConsoleCommand(const ConsoleCommand *command,
                                 uint32_t now_ms);
static void WritePidReport(bool accepted);
static void WriteEncoderResult(void);
static void WriteCanDiagnostics(void);
static void WriteConsoleHelp(void);
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
  ChassisControl_Init();
  if (!BoardSelfTest_Init()) {
    Error_Handler();
  }
  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
    Error_Handler();
  }

#if ENABLE_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = HAL_GetTick();
  if (!SubmitCommand(0, 0, COMMAND_SOURCE_DEMO, demo_stage_started_ms,
                     false, 0U) ||
      !ChassisControl_Start()) {
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
  BoardMotorTestRequest motor_test_request;
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
        ChassisControl_Stop();
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
        (void)ChassisControl_Start();
      }
    } else {
      ChassisControl_Stop();
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
    ChassisControl_LatchInternalFault(CHASSIS_FAULT_CONTROL_OVERRUN);
  } else {
    while (pending_ticks > 0U) {
      ChassisControl_Tick10ms(now_ms);
      --pending_ticks;
      last_control_run_ms = now_ms;
    }
  }

  StatusDisplay_Run(now_ms);
  CanTransport_Run();
  BoardSelfTest_Run(now_ms);

  if (BoardSelfTest_IsIwdgResetRequested()) {
    ChassisControl_Stop();
    BspMotor_EmergencyStop();
  }

  motor_test_request = BoardSelfTest_TakeMotorTestRequest();
  if (motor_test_request != BOARD_MOTOR_TEST_NONE) {
    int16_t left_duty = 0;
    int16_t right_duty = 0;
    bool accepted = false;

    if (motor_test_request == BOARD_MOTOR_TEST_STOP) {
      ChassisControl_Stop();
      accepted = true;
    } else if (!BoardSelfTest_IsIwdgResetRequested()) {
      switch (motor_test_request) {
        case BOARD_MOTOR_TEST_LEFT_FORWARD:
          left_duty = MOTOR_OPEN_LOOP_TEST_DUTY;
          break;
        case BOARD_MOTOR_TEST_LEFT_REVERSE:
          left_duty = -MOTOR_OPEN_LOOP_TEST_DUTY;
          break;
        case BOARD_MOTOR_TEST_RIGHT_FORWARD:
          right_duty = MOTOR_OPEN_LOOP_TEST_DUTY;
          break;
        case BOARD_MOTOR_TEST_RIGHT_REVERSE:
          right_duty = -MOTOR_OPEN_LOOP_TEST_DUTY;
          break;
        default:
          break;
      }
      accepted = ChassisControl_StartOpenLoopTest(
          left_duty, right_duty, now_ms,
          MOTOR_OPEN_LOOP_TEST_DURATION_MS);
    }
    BoardSelfTest_ReportMotorTestResult(motor_test_request, accepted);
  }

  if (Telemetry_IsDue(now_ms)) {
    ChassisControlStatus status;
    TelemetrySnapshot snapshot;
    uint32_t supply_mv;
    const bool supply_valid =
        BspPowerSample_ReadMillivolts(&supply_mv);

    ChassisControl_GetStatus(&status);
    snapshot.left_target = status.left_target;
    snapshot.left_delta = status.left_delta;
    snapshot.left_total = status.left_total;
    snapshot.left_output = status.left_output;
    snapshot.right_target = status.right_target;
    snapshot.right_delta = status.right_delta;
    snapshot.right_total = status.right_total;
    snapshot.right_output = status.right_output;
    snapshot.supply_mv = supply_valid ? (int32_t)supply_mv : -1;
    snapshot.control_state = (uint32_t)status.state;
    snapshot.fault_flags = status.fault_flags;
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
      !ChassisControl_HasInternalFault() &&
      !BoardSelfTest_IsIwdgResetRequested()) {
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
      BoardSelfTest_RequestReport();
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
      WritePidReport(ChassisControl_SetPidGains(
          command->type == CONSOLE_COMMAND_PID_SET_LEFT
              ? CHASSIS_PID_LEFT
              : CHASSIS_PID_RIGHT,
          command->arguments.pid.kp, command->arguments.pid.ki,
          command->arguments.pid.kd));
      break;
    case CONSOLE_COMMAND_PID_TARGET:
      if (SubmitCommand(command->arguments.target.left,
                        command->arguments.target.right,
                        COMMAND_SOURCE_CONSOLE, now_ms, false, 0U)) {
        (void)ChassisControl_Start();
      }
      break;
    case CONSOLE_COMMAND_PID_STOP:
      ChassisControl_Stop();
      break;
    case CONSOLE_COMMAND_ENCODER_ZERO:
      (void)snprintf(response, sizeof(response), "ENCODER_CAL: %s\r\n",
                     ChassisControl_ResetEncoderTotals()
                         ? "RESET"
                         : "REJECTED, stop motor first");
      (void)BspUart_WriteString(response);
      break;
    case CONSOLE_COMMAND_ENCODER_RESULT:
      WriteEncoderResult();
      break;
    case CONSOLE_COMMAND_QSPI_TEST:
      if (!BoardSelfTest_RequestQspiTest()) {
        WriteConsoleHelp();
      }
      break;
    case CONSOLE_COMMAND_IWDG_RESET:
      if (!BoardSelfTest_RequestIwdgReset()) {
        WriteConsoleHelp();
      }
      break;
    case CONSOLE_COMMAND_MOTOR_STOP:
    case CONSOLE_COMMAND_MOTOR_LEFT_FORWARD:
    case CONSOLE_COMMAND_MOTOR_LEFT_REVERSE:
    case CONSOLE_COMMAND_MOTOR_RIGHT_FORWARD:
    case CONSOLE_COMMAND_MOTOR_RIGHT_REVERSE: {
      const BoardMotorTestRequest request =
          command->type == CONSOLE_COMMAND_MOTOR_STOP
              ? BOARD_MOTOR_TEST_STOP
              : command->type == CONSOLE_COMMAND_MOTOR_LEFT_FORWARD
                    ? BOARD_MOTOR_TEST_LEFT_FORWARD
                    : command->type == CONSOLE_COMMAND_MOTOR_LEFT_REVERSE
                          ? BOARD_MOTOR_TEST_LEFT_REVERSE
                          : command->type == CONSOLE_COMMAND_MOTOR_RIGHT_FORWARD
                                ? BOARD_MOTOR_TEST_RIGHT_FORWARD
                                : BOARD_MOTOR_TEST_RIGHT_REVERSE;

      if (!BoardSelfTest_RequestMotorTest(request)) {
        WriteConsoleHelp();
      }
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
  uint16_t left_kp;
  uint16_t left_ki;
  uint16_t left_kd;
  uint16_t right_kp;
  uint16_t right_ki;
  uint16_t right_kd;

  ChassisControl_GetPidGains(CHASSIS_PID_LEFT, &left_kp, &left_ki, &left_kd);
  ChassisControl_GetPidGains(CHASSIS_PID_RIGHT, &right_kp, &right_ki,
                             &right_kd);
  if (accepted) {
    length = snprintf(
        response, sizeof(response),
        "PID: left kp=%u ki=%u kd=%u right kp=%u ki=%u kd=%u\r\n",
        (unsigned int)left_kp, (unsigned int)left_ki,
        (unsigned int)left_kd, (unsigned int)right_kp,
        (unsigned int)right_ki, (unsigned int)right_kd);
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
  ChassisControlStatus status;
  char response[96];
  int length;

  ChassisControl_GetStatus(&status);
  length = snprintf(response, sizeof(response),
                    "ENCODER_CAL: left=%ld right=%ld counts\r\n",
                    (long)status.left_total, (long)status.right_total);
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
  ChassisControl_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
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
    ChassisControl_EmergencyStopFromIsr();
  }
}
