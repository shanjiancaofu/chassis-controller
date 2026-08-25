#include "app/runtime/app_bootstrap.h"

#include <stdio.h>

#include "app/chassis/command_manager.h"
#include "app/chassis/odometry.h"
#include "app/chassis/wheel_controller.h"
#include "app/console/commands.h"
#include "app/console/console.h"
#include "app/diagnostics/board_health.h"
#include "app/diagnostics/diagnostic_report.h"
#include "app/diagnostics/system_status.h"
#include "app/diagnostics/telemetry.h"
#include "app/maintenance/chassis_maintenance.h"
#include "app/maintenance/self_test/iwdg_self_test.h"
#include "app/maintenance/self_test/motor_self_test.h"
#include "app/maintenance/self_test/qspi_self_test.h"
#include "app/parameters/parameter_manager.h"
#include "app/parameters/parameter_storage.h"
#include "app/runtime/control_runtime.h"
#include "app/runtime/diagnostics_runtime.h"
#include "app/runtime/display_runtime.h"
#include "app/runtime/service_runtime.h"
#include "app/safety/fault_manager.h"
#include "app/safety/safety_manager.h"
#include "config/build_info.h"
#include "config/control_config.h"
#include "device.h"
#include "devicetree.h"
#include "drivers/adc/power_sample.h"
#include "drivers/encoder/encoder.h"
#include "drivers/motor/motor.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/time.h"
#include "drivers/uart.h"
#include "drivers/watchdog.h"
#include "subsys/communication/can/can_transport.h"
#include "subsys/communication/can/chassis_protocol.h"
#include "subsys/communication/can/chassis_protocol_ids.h"
#include "subsys/communication/ota/ota_can.h"
#include "subsys/communication/ota/ota_confirmation.h"
#include "subsys/communication/ota/ota_session.h"
#include "subsys/communication/ota/ota_uart.h"
#include "subsys/communication/uart/uart_messages.h"

static const OdometryConfig odometry_config = {
    .encoder_counts_per_revolution = MOTOR_ENCODER_COUNTS_PER_REVOLUTION,
    .wheel_diameter_m = CHASSIS_WHEEL_DIAMETER_M,
    .track_width_m = CHASSIS_TRACK_WIDTH_M,
};

static int SendChassisFrame(const struct can_frame *frame) {
  return CanTransport_Send(frame);
}

static const ChassisProtocolPort chassis_protocol_port = {
    .send = SendChassisFrame,
};

static const ChassisMaintenancePort maintenance_port = {
    .acquire_ota_lock = ControlRuntime_AcquireOtaLock,
    .release_ota_lock = ControlRuntime_ReleaseOtaLock,
};

static const ConsoleCommandPort console_command_port = {
    .submit_motion_command = ControlRuntime_SubmitMotionCommand,
    .start_control = ControlRuntime_Start,
    .stop_control = ControlRuntime_Stop,
    .reset_wheel_odometry = ControlRuntime_ResetOdometry,
    .arm_uart_ota = ChassisMaintenance_ArmUartOta,
    .acquire_self_test_lock = ControlRuntime_AcquireSelfTestLock,
    .start_motor_self_test = ControlRuntime_StartMotorSelfTest,
};

static bool InitializeCommunication(const struct device *can_device,
                                    uint32_t now_ms) {
  static const struct can_filter accepted_filters[] = {
      /* Legacy 0x100 and formal 0x101 control. */
      {.id = CHASSIS_CAN_ID_CMD_WHEEL_RAW, .mask = 0x7FEU},
      {.id = CHASSIS_CAN_ID_HEARTBEAT, .mask = 0x7FFU},
      /* Development handshake 0x720 and OTA request 0x730. */
      {.id = CHASSIS_CAN_ID_DEV_HANDSHAKE_REQUEST, .mask = 0x7EFU},
  };
  if (!device_is_ready(DEVICE_DT_GET(DT_CHOSEN(chassis_uart)))) {
    return false;
  }
  UartMessages_Init();
  Console_Init();
  Telemetry_Init();
  (void)UartMessages_SendLog(now_ms, UART_MESSAGES_LOG_INFO, "boot", "STARTED",
                             "fw=" CHASSIS_FIRMWARE_VERSION
                             " build=" CHASSIS_FIRMWARE_BUILD_STRING);
  if (CanTransport_Init(can_device, accepted_filters,
                        sizeof(accepted_filters) /
                            sizeof(accepted_filters[0])) < 0 ||
      !ChassisProtocol_Init(&chassis_protocol_port)) {
    (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_ERROR,
                               "board", "FDCAN_INIT_FAILED",
                               "code=UNAVAILABLE");
    return false;
  }
  (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_INFO, "board",
                             "FDCAN_READY", NULL);
  OtaCan_Init(can_device);
  OtaUart_Init();
  OtaSession_Init();
  return ChassisMaintenance_Init(&maintenance_port) &&
         ConsoleCommands_Init(&console_command_port);
}

static bool InitializeHardware(const struct device *drive,
                               const struct device *left_encoder,
                               const struct device *right_encoder,
                               const struct device *imu) {
  if (!device_is_ready(drive) || motor_start(drive) < 0) {
    (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_ERROR,
                               "motor", "INIT_FAILED", "code=UNAVAILABLE");
    return false;
  }
  if (!device_is_ready(left_encoder) || !device_is_ready(right_encoder) ||
      encoder_start(left_encoder) < 0 || encoder_start(right_encoder) < 0 ||
      !device_is_ready(DEVICE_DT_GET(DT_CHOSEN(chassis_power)))) {
    (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_ERROR,
                               "board", "MOTION_IO_INIT_FAILED",
                               "code=UNAVAILABLE");
    return false;
  }
  (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_INFO, "board",
                             "MOTION_IO_READY", NULL);
  if (!device_is_ready(DEVICE_DT_GET(DT_CHOSEN(chassis_buttons))) ||
      !device_is_ready(DEVICE_DT_GET(DT_CHOSEN(chassis_estop))) ||
      !device_is_ready(DEVICE_DT_GET(DT_CHOSEN(chassis_watchdog)))) {
    return false;
  }
  (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_INFO, "sr501",
                             "WARMING_UP", "warmup_ms=60000");
  return DiagnosticsRuntime_Init(imu) && DisplayRuntime_Init();
}

static bool InitializeProductModules(void) {
  ParameterSnapshot initial_parameters;
  ParameterStorageSnapshot parameter_storage;
  const bool parameters_loaded = ParameterStorage_Init(&initial_parameters);

  CommandManager_Init();
  FaultManager_Init();
  SafetyManager_Init(
      emergency_stop_is_asserted(DEVICE_DT_GET(DT_CHOSEN(chassis_estop))));
  emergency_stop_set_callback(DEVICE_DT_GET(DT_CHOSEN(chassis_estop)),
                              SafetyManager_LatchEmergencyStopFromIsr);
  BoardHealth_Init();
  ParameterManager_Init(parameters_loaded ? &initial_parameters : NULL);
  if (!ControlRuntime_Init()) {
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
    (void)UartMessages_SendLog(
        time_uptime_ms(),
        parameter_storage.status == PARAMETER_STORAGE_ERROR
            ? UART_MESSAGES_LOG_ERROR
            : UART_MESSAGES_LOG_INFO,
        "parameters", parameters_loaded ? "LOADED" : "DEFAULTS", fields);
  }
  if (!Odometry_Init(&odometry_config)) {
    (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_ERROR,
                               "odometry", "INIT_FAILED",
                               "code=INVALID_GEOMETRY");
    return false;
  }
  (void)UartMessages_SendLog(
      time_uptime_ms(), UART_MESSAGES_LOG_INFO, "odometry", "READY",
      "counts_per_rev=1320 wheel_diameter_mm=65 track_width_mm=220");
  SystemStatus_Init();
  OtaConfirmation_Init();
  QspiSelfTest_Init();
  IwdgSelfTest_Init();
  MotorSelfTest_Init();
  DiagnosticReport_Init(time_uptime_ms());
  return true;
}

bool AppBootstrap_Init(void) {
  const struct device *can_device = DEVICE_DT_GET(DT_CHOSEN(chassis_can));
  const struct device *drive = DEVICE_DT_GET(DT_NODELABEL(drive0));
  const struct device *left_encoder = DEVICE_DT_GET(DT_NODELABEL(left_encoder));
  const struct device *right_encoder =
      DEVICE_DT_GET(DT_NODELABEL(right_encoder));
#if CONFIG_ICM45686
  const struct device *imu = DEVICE_DT_GET(DT_NODELABEL(imu0));
#else
  const struct device *imu = NULL;
#endif
  const uint32_t now_ms = time_uptime_ms();

  if (!ControlRuntime_BindDevices(drive, left_encoder, right_encoder) ||
      !InitializeCommunication(can_device, now_ms) ||
      !InitializeHardware(drive, left_encoder, right_encoder, imu) ||
      !InitializeProductModules()) {
    return false;
  }
  (void)UartMessages_SendLog(time_uptime_ms(), UART_MESSAGES_LOG_INFO,
                             "application", "READY", "tasks=4");
  if (SafetyManager_IsEmergencyStopLatched()) {
    WheelController_EmergencyStop();
  }
  return ServiceRuntime_Init();
}
