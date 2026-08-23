#include "app/chassis_app.h"
#include "app/chassis_console_commands.h"
#include "app/chassis_maintenance.h"
#include "app/system_status_collector.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "drivers/button/button.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/encoder/encoder.h"
#include "drivers/sensor/icm45686.h"
#include "drivers/display/lcd.h"
#include "drivers/led/led.h"
#include "drivers/motor/motor.h"
#include "drivers/adc/power_sample.h"
#include "drivers/reset/reset.h"
#include "drivers/sensor/sr501.h"
#include "drivers/time.h"
#include "drivers/watchdog.h"
#include "communication/can_transport/can_transport.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "communication/ota_transport/ota_confirmation.h"
#include "communication/ota_transport/ota_session.h"
#include "communication/ota_transport/ota_uart_transport.h"
#include "config/app_config.h"
#include "config/build_info.h"
#include "config/control_config.h"
#include "subsys/console/console.h"
#include "subsys/console/diagnostic_report.h"
#include "subsys/settings/parameter_storage.h"
#include "subsys/telemetry/telemetry.h"
#include "communication/uart_protocol/uart_protocol.h"
#include "modules/chassis/command_manager.h"
#include "modules/chassis/odometry.h"
#include "modules/chassis/wheel_controller.h"
#include "modules/diagnostics/board_health.h"
#include "modules/diagnostics/system_status.h"
#include "modules/parameters/parameter_manager.h"
#include "modules/safety/fault_manager.h"
#include "modules/safety/safety_manager.h"
#include "modules/sensors/imu_orientation.h"
#include "devicetree_generated.h"
#include "device.h"
#include "devicetree_generated.h"
#include "drivers/uart.h"
#include "ui/lcd/lcd_status_presenter.h"
#include "tests/target/iwdg_target_test.h"
#include "tests/target/motor_target_test.h"
#include "tests/target/qspi_target_test.h"
#include "task.h"

#if MOTOR_CONTROL_OUTPUT_LIMIT > MOTOR_COMPARE_MAX
#error "MOTOR_CONTROL_OUTPUT_LIMIT exceeds TIM8 compare range"
#endif

static uint32_t consecutive_control_overruns;
static const OdometryConfig odometry_config = {
    .encoder_counts_per_revolution = MOTOR_ENCODER_COUNTS_PER_REVOLUTION,
    .wheel_diameter_m = CHASSIS_WHEEL_DIAMETER_M,
    .track_width_m = CHASSIS_TRACK_WIDTH_M,
};

static const struct device *drive_device;
static const struct device *left_encoder_device;
static const struct device *right_encoder_device;
static const struct device *imu_device;

static int16_t GetLeftMotorAppliedDuty(void)
{
  return motor_get_applied_duty(drive_device, MOTOR_LEFT);
}

static int16_t GetRightMotorAppliedDuty(void)
{
  return motor_get_applied_duty(drive_device, MOTOR_RIGHT);
}

static void CoastMotors(void)
{
  motor_coast_all(drive_device);
}

static void EmergencyStopMotors(void)
{
  motor_emergency_stop(drive_device);
}

static void SetMotorDutyBoth(int16_t left_duty, int16_t right_duty)
{
  motor_set_signed_duty_both(drive_device, left_duty, right_duty);
}

static const WheelControllerMotorPort wheel_controller_motor_port = {
    .coast_all = CoastMotors,
    .emergency_stop = EmergencyStopMotors,
    .set_signed_duty_both = SetMotorDutyBoth,
    .get_left_applied_duty = GetLeftMotorAppliedDuty,
    .get_right_applied_duty = GetRightMotorAppliedDuty,
};

static void ProcessImuSample(const Icm45686Stm32Sample *sample)
{
  if (sample != NULL) {
    ImuOrientation_ProcessSample(sample->accel_mps2, sample->gyro_rad_s,
                                 sample->sample_period_s);
  }
}

static const Icm45686Stm32SampleSink imu_sample_sink = {
    .reset = ImuOrientation_Reset,
    .process_sample = ProcessImuSample,
};

static bool StartControl(void);
static void StopControl(void);
static void ReleaseMotionOwner(void);
static bool StartMotorTargetTest(MotorTargetTestAction action,
                                 uint32_t now_ms);
static bool AcquireTargetTestLock(void);
static bool AcquireOtaMaintenanceLock(void);
static void ReleaseOtaMaintenanceLock(void);
static void ReleaseFinishedTargetTestLock(void);
static void LatchChassisInternalFault(uint32_t fault);
static void ApplyPendingControlParameters(void);
static bool ResetWheelOdometry(void);
static CommandManagerSubmitResult SubmitMotionCommand(
    int32_t left_target, int32_t right_target, CommandSource source,
    uint32_t now_ms, bool has_sequence, uint8_t sequence);
static const ChassisMaintenancePort maintenance_port = {
    .acquire_ota_lock = AcquireOtaMaintenanceLock,
    .release_ota_lock = ReleaseOtaMaintenanceLock,
};

static const ChassisConsoleCommandPort console_command_port = {
    .submit_motion_command = SubmitMotionCommand,
    .start_control = StartControl,
    .stop_control = StopControl,
    .reset_wheel_odometry = ResetWheelOdometry,
    .arm_uart_ota = ChassisMaintenance_ArmUartOta,
    .acquire_target_test_lock = AcquireTargetTestLock,
    .start_motor_target_test = StartMotorTargetTest,
};

#if CONFIG_MOTOR_DEMO
static uint8_t demo_stage;
static uint32_t demo_stage_started_ms;
#endif

static bool InitializeCommunication(const struct device *can_device,
                                    uint32_t now_ms)
{
  if (!device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_UART))) {
    return false;
  }
  UartProtocol_Init();
  Console_Init();
  Telemetry_Init();
  (void)UartProtocol_SendLog(now_ms, UART_PROTOCOL_LOG_INFO, "boot",
                             "STARTED", "fw=" CHASSIS_FIRMWARE_VERSION
                             " build=" CHASSIS_FIRMWARE_BUILD_STRING);
  if (CanTransport_Init(can_device) < 0) {
    (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_ERROR, "board",
                               "FDCAN_INIT_FAILED", "code=UNAVAILABLE");
    return false;
  }
  (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_INFO, "board",
                             "FDCAN_READY", NULL);
  OtaCanTransport_Init(can_device);
  OtaUartTransport_Init();
  OtaSession_Init();
  return ChassisMaintenance_Init(&maintenance_port) &&
         ChassisConsoleCommands_Init(&console_command_port);
}

static bool InitializeHardware(uint32_t now_ms)
{
  if (!device_is_ready(drive_device) || motor_start(drive_device) < 0) {
    (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_ERROR, "motor",
                               "INIT_FAILED", "code=UNAVAILABLE");
    return false;
  }
  if (!device_is_ready(left_encoder_device) ||
      !device_is_ready(right_encoder_device) ||
      encoder_start(left_encoder_device) < 0 ||
      encoder_start(right_encoder_device) < 0 ||
      !device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_POWER))) {
    (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_ERROR, "board",
                               "MOTION_IO_INIT_FAILED", "code=UNAVAILABLE");
    return false;
  }
  (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_INFO, "board",
                             "MOTION_IO_READY", NULL);
  if (!device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_BUTTONS))) return false;
  (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_INFO, "sr501",
                             "WARMING_UP", "warmup_ms=60000");
#if CONFIG_ICM45686
  {
    Icm45686Stm32Snapshot imu;
    char fields[48];

    ImuOrientation_Init();
    if (!device_is_ready(imu_device) ||
        !sensor_set_sample_sink(imu_device, &imu_sample_sink)) {
      return false;
    }
    sensor_get_snapshot(imu_device, &imu);
    (void)snprintf(fields, sizeof(fields),
                   "device=ICM45686 who_am_i=0x%02X",
                   (unsigned int)imu.who_am_i);
    if (imu.status == ICM45686_READY) {
      (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_INFO, "imu",
                                 "READY", fields);
    } else if (imu.status == ICM45686_NOT_FOUND) {
      (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_WARN, "imu",
                                 "NOT_FOUND", fields);
    } else {
      (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_ERROR, "imu",
                                 "INIT_FAILED", fields);
    }
  }
#endif
  return LcdStatusPresenter_Init();
}

static bool InitializeProductModules(void)
{
  ParameterSnapshot initial_parameters;
  ParameterStorageSnapshot parameter_storage;
  const bool parameters_loaded = ParameterStorage_Init(&initial_parameters);

  CommandManager_Init();
  FaultManager_Init();
  SafetyManager_Init(emergency_stop_is_asserted(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_ESTOP)));
  emergency_stop_set_callback(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_ESTOP), SafetyManager_LatchEmergencyStopFromIsr);
  BoardHealth_Init();
  ParameterManager_Init(parameters_loaded ? &initial_parameters : NULL);
  if (!WheelController_Init(&wheel_controller_motor_port)) {
    return false;
  }
  ParameterManager_GetActive(&initial_parameters);
  WheelController_ApplyPidGains(
      WHEEL_CONTROLLER_LEFT, initial_parameters.left_pid.kp,
      initial_parameters.left_pid.ki, initial_parameters.left_pid.kd);
  WheelController_ApplyPidGains(
      WHEEL_CONTROLLER_RIGHT, initial_parameters.right_pid.kp,
      initial_parameters.right_pid.ki, initial_parameters.right_pid.kd);
  ParameterStorage_GetSnapshot(&parameter_storage);
  {
    char fields[128];

    (void)snprintf(fields, sizeof(fields),
                   "source=%s sequence=%lu left=%u,%u,%u right=%u,%u,%u",
                   parameters_loaded ? "QSPI" : "DEFAULTS",
                   (unsigned long)parameter_storage.sequence,
                   (unsigned int)initial_parameters.left_pid.kp,
                   (unsigned int)initial_parameters.left_pid.ki,
                   (unsigned int)initial_parameters.left_pid.kd,
                   (unsigned int)initial_parameters.right_pid.kp,
                   (unsigned int)initial_parameters.right_pid.ki,
                   (unsigned int)initial_parameters.right_pid.kd);
    (void)UartProtocol_SendLog(
        time_uptime_ms(),
        parameter_storage.status == PARAMETER_STORAGE_ERROR
            ? UART_PROTOCOL_LOG_ERROR
            : UART_PROTOCOL_LOG_INFO,
        "parameters", parameters_loaded ? "LOADED" : "DEFAULTS", fields);
  }
  if (!Odometry_Init(&odometry_config)) {
    (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_ERROR,
                               "odometry", "INIT_FAILED",
                               "code=INVALID_GEOMETRY");
    return false;
  }
  (void)UartProtocol_SendLog(
      time_uptime_ms(), UART_PROTOCOL_LOG_INFO, "odometry", "READY",
      "counts_per_rev=1320 wheel_diameter_mm=65 track_width_mm=220");
  SystemStatus_Init();
  OtaConfirmation_Init();
  QspiTargetTest_Init();
  IwdgTargetTest_Init();
  MotorTargetTest_Init();
  DiagnosticReport_Init(time_uptime_ms());
  return true;
}

bool ChassisApp_Init(void)
{
  const struct device *can_device = DEVICE_DT_GET(DT_CHOSEN_CHASSIS_CAN);
  const uint32_t now_ms = time_uptime_ms();
  drive_device = DEVICE_DT_GET(DT_NODE_DRIVE0);
  left_encoder_device = DEVICE_DT_GET(DT_NODE_LEFT_ENCODER);
  right_encoder_device = DEVICE_DT_GET(DT_NODE_RIGHT_ENCODER);
  imu_device = DEVICE_DT_GET(DT_NODE_IMU0);

  if (!InitializeCommunication(can_device, now_ms) ||
      !InitializeHardware(now_ms) || !InitializeProductModules()) {
    return false;
  }
  (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_INFO,
                             "application", "READY", "tasks=4");
  consecutive_control_overruns = 0U;
  if (SafetyManager_IsEmergencyStopLatched()) {
    WheelController_EmergencyStop();
  }
#if CONFIG_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = time_uptime_ms();
  if (SubmitMotionCommand(0, 0, COMMAND_SOURCE_TARGET_TEST,
                          demo_stage_started_ms, false, 0U) !=
          COMMAND_SUBMIT_ACCEPTED ||
      !StartControl()) {
    return false;
  }
#endif
  return true;
}

void ChassisApp_RunServiceCycle(void)
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
  if (demo_stage < 6U) {
    taskENTER_CRITICAL();
    (void)CommandManager_Refresh(COMMAND_SOURCE_TARGET_TEST, now_ms);
    taskEXIT_CRITICAL();
  }
  switch (demo_stage) {
    case 0U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitMotionCommand(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                  MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                  COMMAND_SOURCE_TARGET_TEST, now_ms, false, 0U);
        demo_stage = 1U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 1U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)SubmitMotionCommand(0, 0, COMMAND_SOURCE_TARGET_TEST, now_ms,
                                  false, 0U);
        demo_stage = 2U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 2U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitMotionCommand(-MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                  -MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                  COMMAND_SOURCE_TARGET_TEST, now_ms, false, 0U);
        demo_stage = 3U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 3U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        (void)SubmitMotionCommand(0, 0, COMMAND_SOURCE_TARGET_TEST, now_ms,
                                  false, 0U);
        demo_stage = 4U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 4U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        (void)SubmitMotionCommand(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
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
  ChassisMaintenance_Run(now_ms);
  if (CanTransport_TakeControlCommand(&control_command)) {
    if (control_command.enabled) {
      if (SubmitMotionCommand(control_command.left_target,
                              control_command.right_target,
                              COMMAND_SOURCE_CAN_REMOTE, now_ms, true,
                              control_command.sequence) ==
          COMMAND_SUBMIT_ACCEPTED) {
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

  QspiTargetTest_Run(now_ms);
  {
    SystemStatusSnapshot status;

    SystemStatus_GetSnapshot(&status);
    OtaConfirmation_Run(
        now_ms,
        status.board_health.qspi_id_valid &&
            status.runtime.critical_tasks_healthy &&
            !FaultManager_HasCritical(),
        QspiTargetTest_GetStatus() != QSPI_TARGET_TEST_RUNNING &&
            !OtaSession_IsUsingQspi() &&
            !ParameterStorage_IsUsingQspi());
  }
  ParameterStorage_Run(
      now_ms,
      QspiTargetTest_GetStatus() != QSPI_TARGET_TEST_RUNNING &&
          !OtaSession_IsUsingQspi() &&
          !OtaConfirmation_IsUsingQspi());
  {
    bool save_success;

    if (ParameterStorage_TakeCompletion(&save_success)) {
      ParameterStorageSnapshot storage;
      char fields[64];

      ParameterStorage_GetSnapshot(&storage);
      (void)snprintf(fields, sizeof(fields),
                     "sequence=%lu error_count=%lu",
                     (unsigned long)storage.sequence,
                     (unsigned long)storage.error_count);
      (void)UartProtocol_SendLog(
          now_ms,
          save_success ? UART_PROTOCOL_LOG_INFO : UART_PROTOCOL_LOG_ERROR,
          "parameters", save_success ? "SAVED" : "SAVE_FAILED", fields);
    }
  }
  if (OtaSession_HasCriticalFault() ||
      OtaConfirmation_HasCriticalFault() ||
      QspiTargetTest_HasCriticalFault()) {
    LatchChassisInternalFault(CHASSIS_FAULT_INTERNAL);
  }
  MotorTargetTest_Run(now_ms);
  ReleaseFinishedTargetTestLock();
  if (!OtaUartTransport_IsEnabled()) {
    DiagnosticReport_Run(now_ms);
  }

  if (IwdgTargetTest_IsResetRequested()) {
    StopControl();
    motor_emergency_stop(drive_device);
  }

  if (OtaSession_IsResetRequested(now_ms) &&
      ((OtaSession_GetSource() == OTA_SOURCE_UART &&
        OtaUartTransport_IsTxIdle()) ||
       (OtaSession_GetSource() == OTA_SOURCE_CAN_FD &&
        OtaCanTransport_IsTxIdle()))) {
    StopControl();
    motor_coast_all(drive_device);
    if (watchdog_prepare_for_bootloader()) {
      Reset_RequestSystemReset();
    }
  }

  if (!OtaUartTransport_IsEnabled() && Telemetry_IsDue(now_ms)) {
    WheelControllerSnapshot wheel_snapshot;
    MotorTargetTestSnapshot motor_test_snapshot;
    OdometrySnapshot odometry_snapshot;
    TelemetrySnapshot snapshot;
    uint32_t supply_mv;
    const bool supply_valid =
        power_sample_read_millivolts(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_POWER), &supply_mv);

    taskENTER_CRITICAL();
    WheelController_GetSnapshot(&wheel_snapshot);
    MotorTargetTest_GetSnapshot(&motor_test_snapshot);
    Odometry_GetSnapshot(now_ms, &odometry_snapshot);
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

void ChassisApp_RunDiagnosticsCycle(void)
{
  static uint32_t last_heartbeat_ms;
  const uint32_t now_ms = time_uptime_ms();

  sr501_run(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_SR501), now_ms);
#if CONFIG_ICM45686
  sensor_run(imu_device, now_ms);
#endif
  SystemStatusCollector_Update(now_ms);

  if (now_ms - last_heartbeat_ms >= 500U) {
    last_heartbeat_ms = now_ms;
    led_toggle(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_LEDS), LED_BLUE);
  }
  led_set(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_LEDS), LED_GREEN, false);
  led_set(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_LEDS), LED_RED, false);
  if (CanTransport_GetLinkStatus() == CAN_TRANSPORT_LINK_PASSED) {
    led_set(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_LEDS), LED_GREEN, true);
  } else if (CanTransport_GetLinkStatus() == CAN_TRANSPORT_LINK_FAILED) {
    led_set(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_LEDS), LED_RED, true);
  }

  {
    SystemStatusSnapshot status;

    SystemStatus_GetSnapshot(&status);
    if (status.runtime.critical_tasks_healthy &&
        !FaultManager_HasCritical() &&
        CanTransport_IsOperational() &&
        !IwdgTargetTest_IsResetRequested()) {
      (void)watchdog_refresh();
    }
  }
}

void ChassisApp_RunDisplayCycle(void)
{
  const uint32_t now_ms = time_uptime_ms();

  button_run(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_BUTTONS), now_ms);
  LcdStatusPresenter_Run(now_ms);
}

static bool StartControl(void)
{
  CommandManagerCommand command;
  bool accepted;

  taskENTER_CRITICAL();
  accepted = CommandManager_Get(&command) &&
             SafetyManager_RequestRun(true);
  taskEXIT_CRITICAL();
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

void ChassisApp_RunControlCycle(uint32_t notification_count)
{
  CommandManagerCommand command;
  int32_t left_delta;
  int32_t left_measurement;
  int32_t right_delta;
  int32_t right_measurement;
  uint32_t latest_supply_mv;
  int64_t max_encoder_delta;
  int64_t left_abs_delta;
  int64_t right_abs_delta;
  bool command_available;
  uint32_t missed_ticks;
  const uint32_t now_ms = time_uptime_ms();

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
      LatchChassisInternalFault(CHASSIS_FAULT_CONTROL_OVERRUN);
      return;
    }
  } else {
    consecutive_control_overruns = 0U;
  }

  if (encoder_read_delta(left_encoder_device, &left_delta) < 0 ||
      encoder_read_delta(right_encoder_device, &right_delta) < 0) {
    left_delta = 0;
    right_delta = 0;
  }
  max_encoder_delta =
      (int64_t)MOTOR_ENCODER_MAX_DELTA_PER_TICK * notification_count;
  left_abs_delta = left_delta < 0 ? -(int64_t)left_delta : left_delta;
  right_abs_delta = right_delta < 0 ? -(int64_t)right_delta : right_delta;
  if (left_abs_delta > max_encoder_delta ||
      right_abs_delta > max_encoder_delta) {
    LatchChassisInternalFault(CHASSIS_FAULT_ENCODER);
    return;
  }
  if (power_sample_get_latest_millivolts(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_POWER), &latest_supply_mv) &&
      latest_supply_mv < MOTOR_CONTROL_MIN_SUPPLY_MV) {
    LatchChassisInternalFault(CHASSIS_FAULT_UNDERVOLTAGE);
    return;
  }
  if (!Odometry_Update(left_delta, right_delta, now_ms,
                       MOTOR_CONTROL_PERIOD_MS * notification_count)) {
    LatchChassisInternalFault(CHASSIS_FAULT_INTERNAL);
    return;
  }
  left_measurement = left_delta / (int32_t)notification_count;
  right_measurement = right_delta / (int32_t)notification_count;
  ApplyPendingControlParameters();
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
                              left_measurement, right_measurement,
                              notification_count)) {
    LatchChassisInternalFault(CHASSIS_FAULT_INTERNAL);
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

static bool AcquireTargetTestLock(void)
{
  MotorTargetTestSnapshot motor_test;

  MotorTargetTest_GetSnapshot(&motor_test);
  if (IwdgTargetTest_IsResetRequested() ||
      QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING ||
      OtaConfirmation_IsUsingQspi() ||
      ParameterStorage_IsUsingQspi() ||
      OtaSession_IsActive() || OtaUartTransport_IsEnabled() ||
      motor_test.running) {
    return false;
  }

  StopControl();
  motor_coast_all(drive_device);
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED ||
      motor_get_applied_duty(drive_device, MOTOR_LEFT) != 0 ||
      motor_get_applied_duty(drive_device, MOTOR_RIGHT) != 0) {
    return false;
  }

  taskENTER_CRITICAL();
  ReleaseMotionOwner();
  const bool acquired =
      CommandManager_Acquire(COMMAND_SOURCE_TARGET_TEST);
  taskEXIT_CRITICAL();
  return acquired;
}

static bool AcquireOtaMaintenanceLock(void)
{
  MotorTargetTestSnapshot motor_test;

  MotorTargetTest_GetSnapshot(&motor_test);
  if (IwdgTargetTest_IsResetRequested() ||
      QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING ||
      OtaConfirmation_IsUsingQspi() ||
      ParameterStorage_IsUsingQspi() || OtaSession_IsActive() ||
      motor_test.running) {
    return false;
  }

  StopControl();
  motor_coast_all(drive_device);
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED ||
      motor_get_applied_duty(drive_device, MOTOR_LEFT) != 0 ||
      motor_get_applied_duty(drive_device, MOTOR_RIGHT) != 0) {
    return false;
  }

  taskENTER_CRITICAL();
  ReleaseMotionOwner();
  const bool acquired = CommandManager_Acquire(COMMAND_SOURCE_OTA);
  taskEXIT_CRITICAL();
  return acquired;
}

static void ReleaseOtaMaintenanceLock(void)
{
  taskENTER_CRITICAL();
  CommandManager_Release(COMMAND_SOURCE_OTA);
  taskEXIT_CRITICAL();
}


static void ReleaseFinishedTargetTestLock(void)
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

static void LatchChassisInternalFault(uint32_t fault)
{
  FaultManager_Raise(fault);
  SafetyManager_LatchInternalFault();
  taskENTER_CRITICAL();
  CommandManager_ClearCommand();
  ReleaseMotionOwner();
  WheelController_EmergencyStop();
  taskEXIT_CRITICAL();
}

static void ApplyPendingControlParameters(void)
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

static bool ResetWheelOdometry(void)
{
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED) {
    return false;
  }

  taskENTER_CRITICAL();
  Odometry_Reset();
  taskEXIT_CRITICAL();
  return true;
}

static CommandManagerSubmitResult SubmitMotionCommand(
    int32_t left_target, int32_t right_target, CommandSource source,
    uint32_t now_ms, bool has_sequence, uint8_t sequence)
{
  CommandManagerSubmitResult result;
  const CommandManagerCommand command = {
      .left_target = left_target,
      .right_target = right_target,
      .received_ms = now_ms,
      .source = source,
      .sequence = sequence,
      .has_sequence = has_sequence,
  };

  taskENTER_CRITICAL();
  result = CommandManager_Submit(&command);
  taskEXIT_CRITICAL();
  return result;
}

void ChassisApp_FatalError(void)
{
  motor_emergency_stop(drive_device);
  LatchChassisInternalFault(CHASSIS_FAULT_INTERNAL);
}

void ChassisApp_PanicStopFromException(void)
{
  taskDISABLE_INTERRUPTS();
  motor_emergency_stop(drive_device);
}

bool ChassisApp_ClearEmergencyStop(void)
{
  if (emergency_stop_is_asserted(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_ESTOP))) {
    return false;
  }

  taskENTER_CRITICAL();
  if (emergency_stop_is_asserted(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_ESTOP))) {
    taskEXIT_CRITICAL();
    return false;
  }
  (void)SafetyManager_ClearEmergencyStop();
  motor_clear_emergency_stop(drive_device);
  taskEXIT_CRITICAL();

  StopControl();
  return true;
}
