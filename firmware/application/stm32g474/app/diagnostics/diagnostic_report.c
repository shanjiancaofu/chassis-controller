#include "app/diagnostics/diagnostic_report.h"

#include <limits.h>
#include <stdio.h>

#include "drivers/reset/reset.h"
#include "config/build_info.h"
#include "config/storage_layout.h"
#include "subsys/communication/ota_transport/ota_confirmation.h"
#include "app/diagnostics/telemetry.h"
#include "subsys/communication/uart_protocol/uart_protocol.h"
#include "app/diagnostics/system_status.h"
#include "app/maintenance/self_test/qspi_self_test.h"

#define SELF_TEST_REPORT_DELAY_MS 100U

static uint32_t initialized_ms;
static bool initial_report_sent;
static bool self_test_report_requested;
static bool qspi_report_requested;
static bool iwdg_report_requested;
static char system_fields[1400];
static char motor_fields[768];
static char sensor_fields[1024];
static char communication_fields[768];

static bool WriteSelfTestReport(uint32_t now_ms);
static bool WriteQspiTestReport(uint32_t now_ms);
static void FormatEncoderCount(char *buffer, size_t capacity, int64_t value);
static int32_t FixedFromFloat(float value, float scale);
static const char *TaskStateText(SystemStatusTaskState state);
static const char *ResetCauseText(uint32_t flags);
static const char *ControlStateText(ChassisControlState state);
static const char *CanStateText(SystemStatusCanState state);
static const char *LcdStateText(SystemStatusLcdState state);
static const char *ImuStateText(const SystemStatusImuSnapshot *snapshot);
static const char *OtaConfirmationText(uint32_t state);
static const char *TelemetryModeText(uint32_t mode);

void DiagnosticReport_Init(uint32_t now_ms)
{
  initialized_ms = now_ms;
  initial_report_sent = false;
  self_test_report_requested = false;
  qspi_report_requested = false;
  iwdg_report_requested = false;
}

void DiagnosticReport_Run(uint32_t now_ms)
{
  if (QspiSelfTest_TakeCompletion()) {
    qspi_report_requested = true;
  }
  if ((!initial_report_sent &&
       (now_ms - initialized_ms >= SELF_TEST_REPORT_DELAY_MS)) ||
      self_test_report_requested) {
    if (WriteSelfTestReport(now_ms)) {
      initial_report_sent = true;
      self_test_report_requested = false;
    }
    return;
  }
  if (qspi_report_requested && WriteQspiTestReport(now_ms)) {
    qspi_report_requested = false;
    return;
  }
  if (iwdg_report_requested &&
      UartProtocol_SendLog(now_ms, UART_PROTOCOL_LOG_WARN, "iwdg",
                           "RESET_TEST_ARMED",
                           "state=ARMED timeout_ms=10000")) {
    iwdg_report_requested = false;
  }
}

void DiagnosticReport_RequestSelfTest(void)
{
  self_test_report_requested = true;
}

void DiagnosticReport_RequestQspiTest(void)
{
  qspi_report_requested = true;
}

void DiagnosticReport_RequestIwdgArmed(void)
{
  iwdg_report_requested = true;
}

static bool WriteSelfTestReport(uint32_t now_ms)
{
  SystemStatusSnapshot status;
  UartProtocolTelemetryLine lines[4];
  const uint32_t sequence = UartProtocol_NextTelemetrySequence();
  char left_encoder[24];
  char right_encoder[24];

  SystemStatus_GetSnapshot(&status);
  FormatEncoderCount(left_encoder, sizeof(left_encoder),
                     status.odometry.left_total);
  FormatEncoderCount(right_encoder, sizeof(right_encoder),
                     status.odometry.right_total);

  (void)snprintf(
      system_fields, sizeof(system_fields),
      "fw=%s build=%u uptime_ms=%lu supply_valid=%u supply_mv=%lu control=%s "
      "fault=0x%08lx reset=%s reset_flags=0x%08lx critical_tasks=%u "
      "service_task=%s control_task=%s diagnostics_task=%s display_task=%s "
      "service_period_ms=%lu service_expected_ms=%lu service_timeout_ms=%lu "
      "service_age_ms=%lu service_runs=%lu service_stack_free_words=%lu "
      "control_period_ms=%lu control_expected_ms=%lu control_timeout_ms=%lu "
      "control_age_ms=%lu control_runs=%lu control_stack_free_words=%lu "
      "diagnostics_period_ms=%lu diagnostics_expected_ms=%lu "
      "diagnostics_timeout_ms=%lu diagnostics_age_ms=%lu diagnostics_runs=%lu "
      "diagnostics_stack_free_words=%lu display_period_ms=%lu "
      "display_expected_ms=%lu display_timeout_ms=%lu display_age_ms=%lu "
      "display_runs=%lu display_stack_free_words=%lu",
      CHASSIS_FIRMWARE_VERSION, (unsigned int)CHASSIS_FIRMWARE_BUILD,
      (unsigned long)status.runtime.uptime_ms,
      status.supply_valid ? 1U : 0U, (unsigned long)status.supply_mv,
      ControlStateText(status.control_state),
      (unsigned long)status.fault_flags,
      ResetCauseText(status.board_health.reset_cause_flags),
      (unsigned long)status.board_health.reset_cause_flags,
      status.runtime.critical_tasks_healthy ? 1U : 0U,
      TaskStateText(status.runtime.service_task_state),
      TaskStateText(status.runtime.control_task_state),
      TaskStateText(status.runtime.diagnostics_task_state),
      TaskStateText(status.runtime.display_task_state),
      (unsigned long)status.runtime.service_period_ms,
      (unsigned long)status.runtime.service_expected_period_ms,
      (unsigned long)status.runtime.service_timeout_ms,
      (unsigned long)status.runtime.service_heartbeat_age_ms,
      (unsigned long)status.runtime.service_run_count,
      (unsigned long)status.runtime.service_stack_high_water_words,
      (unsigned long)status.runtime.control_period_ms,
      (unsigned long)status.runtime.control_expected_period_ms,
      (unsigned long)status.runtime.control_timeout_ms,
      (unsigned long)status.runtime.control_heartbeat_age_ms,
      (unsigned long)status.runtime.control_run_count,
      (unsigned long)status.runtime.control_stack_high_water_words,
      (unsigned long)status.runtime.diagnostics_period_ms,
      (unsigned long)status.runtime.diagnostics_expected_period_ms,
      (unsigned long)status.runtime.diagnostics_timeout_ms,
      (unsigned long)status.runtime.diagnostics_heartbeat_age_ms,
      (unsigned long)status.runtime.diagnostics_run_count,
      (unsigned long)status.runtime.diagnostics_stack_high_water_words,
      (unsigned long)status.runtime.display_period_ms,
      (unsigned long)status.runtime.display_expected_period_ms,
      (unsigned long)status.runtime.display_timeout_ms,
      (unsigned long)status.runtime.display_heartbeat_age_ms,
      (unsigned long)status.runtime.display_run_count,
      (unsigned long)status.runtime.display_stack_high_water_words);

  (void)snprintf(
      motor_fields, sizeof(motor_fields),
      "control=%s left_target=%ld left_speed=%ld left_pwm=%d "
      "right_target=%ld right_speed=%ld right_pwm=%d left_encoder=%s "
      "right_encoder=%s left_kp=%u left_ki=%u left_kd=%u right_kp=%u "
      "right_ki=%u right_kd=%u motor_test=%u overrun=%lu missed=%lu "
      "odom_valid=%u odom_ts_ms=%lu odom_period_ms=%lu odom_age_ms=%lu "
      "odom_x_mm=%ld odom_y_mm=%ld odom_heading_mrad=%ld "
      "odom_linear_mm_s=%ld odom_angular_mrad_s=%ld",
      ControlStateText(status.control_state),
      (long)status.wheels.left_target,
      (long)status.wheels.left_measurement,
      (int)status.wheels.left_output,
      (long)status.wheels.right_target,
      (long)status.wheels.right_measurement,
      (int)status.wheels.right_output,
      left_encoder,
      right_encoder,
      (unsigned int)status.parameters.left_pid.kp,
      (unsigned int)status.parameters.left_pid.ki,
      (unsigned int)status.parameters.left_pid.kd,
      (unsigned int)status.parameters.right_pid.kp,
      (unsigned int)status.parameters.right_pid.ki,
      (unsigned int)status.parameters.right_pid.kd,
      status.motor_test.running ? 1U : 0U,
      (unsigned long)status.board_health.control_overrun_count,
      (unsigned long)status.board_health.control_missed_tick_count,
      status.odometry.valid ? 1U : 0U,
      (unsigned long)status.odometry.sample_timestamp_ms,
      (unsigned long)status.odometry.sample_period_ms,
      status.odometry.valid
          ? (unsigned long)(now_ms - status.odometry.sample_timestamp_ms)
          : 0UL,
      (long)FixedFromFloat(status.odometry.x_m, 1000.0f),
      (long)FixedFromFloat(status.odometry.y_m, 1000.0f),
      (long)FixedFromFloat(status.odometry.heading_rad, 1000.0f),
      (long)FixedFromFloat(status.odometry.linear_velocity_mps, 1000.0f),
      (long)FixedFromFloat(status.odometry.angular_velocity_rad_s, 1000.0f));

  (void)snprintf(
      sensor_fields, sizeof(sensor_fields),
      "rtc_valid=%u rtc=20%02u-%02u-%02uT%02u:%02u:%02u "
      "adc_valid=%u adc_mv=%lu adc_ts_ms=%lu adc_age_ms=%lu "
      "imu=%s imu_whoami=0x%02X imu_samples=%lu "
      "imu_fifo_frames=%lu imu_fifo_errors=%lu imu_timestamp_errors=%lu "
      "imu_fifo_ts=%u imu_ts_ms=%lu imu_age_ms=%lu "
      "imu_kalman=%u imu_kalman_roll_mrad=%ld imu_kalman_pitch_mrad=%ld "
      "sr501=%s sr501_raw=%u sr501_motion=%u sr501_count=%lu "
      "sr501_last_ms=%lu sr501_warmup_ms=%lu button1_pressed=%u "
      "button1_count=%lu button2_pressed=%u button2_count=%lu",
      status.rtc_valid ? 1U : 0U, (unsigned int)status.rtc_year,
      (unsigned int)status.rtc_month, (unsigned int)status.rtc_date,
      (unsigned int)status.rtc_hours, (unsigned int)status.rtc_minutes,
      (unsigned int)status.rtc_seconds, status.supply_valid ? 1U : 0U,
      (unsigned long)status.supply_mv,
      (unsigned long)status.power_sample.sample_timestamp_ms,
      status.power_sample.valid
          ? (unsigned long)(now_ms - status.power_sample.sample_timestamp_ms)
          : 0UL,
      ImuStateText(&status.imu),
      status.imu.who_am_i, (unsigned long)status.imu.sample_count,
      (unsigned long)status.imu.fifo_frame_count,
      (unsigned long)status.imu.fifo_parse_error_count,
      (unsigned long)status.imu.timestamp_error_count,
      (unsigned int)status.imu.fifo_timestamp,
      (unsigned long)status.imu.last_sample_ms,
      status.imu.sample_valid
          ? (unsigned long)(now_ms - status.imu.last_sample_ms)
          : 0UL,
      status.imu.kalman_valid ? 1U : 0U,
      (long)(status.imu.kalman_roll_rad * 1000.0f),
      (long)(status.imu.kalman_pitch_rad * 1000.0f),
      status.sr501.status == SYSTEM_STATUS_SR501_READY ? "READY"
                                                       : "WARMING_UP",
      status.sr501.raw_high ? 1U : 0U,
      status.sr501.motion_detected ? 1U : 0U,
      (unsigned long)status.sr501.event_count,
      (unsigned long)status.sr501.last_motion_ms,
      (unsigned long)status.sr501.warmup_remaining_ms,
      status.buttons.pressed[SYSTEM_STATUS_BUTTON_1] ? 1U : 0U,
      (unsigned long)status.buttons.pressed_count[SYSTEM_STATUS_BUTTON_1],
      status.buttons.pressed[SYSTEM_STATUS_BUTTON_2] ? 1U : 0U,
      (unsigned long)status.buttons.pressed_count[SYSTEM_STATUS_BUTTON_2]);

  (void)snprintf(
      communication_fields, sizeof(communication_fields),
      "can=%s can_drops=%lu uart_errors=%lu qspi_read=%u qspi_id=%u "
      "qspi_jedec=%02X%02X%02X qspi_capacity_bytes=%lu qspi_test=%lu "
      "ota_confirmation=%s ota_source=%u ota_state=%u ota_offset=%lu "
      "lcd=%s telemetry=%s iwdg_test=%u",
      CanStateText(status.can_state),
      (unsigned long)status.can_drop_count,
      (unsigned long)status.uart_error_count,
      status.board_health.qspi_read_ok ? 1U : 0U,
      status.board_health.qspi_id_valid ? 1U : 0U,
      status.board_health.qspi_jedec_id[0],
      status.board_health.qspi_jedec_id[1],
      status.board_health.qspi_jedec_id[2],
      (unsigned long)status.board_health.qspi_capacity_bytes,
      (unsigned long)status.qspi_test_state,
      OtaConfirmationText(status.ota_confirmation_state),
      (unsigned int)status.ota_source, (unsigned int)status.ota_state,
      (unsigned long)status.ota_next_offset, LcdStateText(status.lcd_state),
      TelemetryModeText(status.telemetry_mode),
      status.board_health.iwdg_reset_test_passed ? 1U : 0U);

  lines[0] = (UartProtocolTelemetryLine){.section = "system",
                                        .fields = system_fields};
  lines[1] = (UartProtocolTelemetryLine){.section = "motor",
                                        .fields = motor_fields};
  lines[2] = (UartProtocolTelemetryLine){.section = "sensors",
                                        .fields = sensor_fields};
  lines[3] = (UartProtocolTelemetryLine){.section = "communication",
                                        .fields = communication_fields};
  return UartProtocol_SendTelemetryBlock(now_ms, sequence, lines, 4U);
}

static bool WriteQspiTestReport(uint32_t now_ms)
{
  const QspiSelfTestStatus status = QspiSelfTest_GetStatus();
  const char *result = status == QSPI_SELF_TEST_RUNNING
                           ? "RUNNING"
                           : status == QSPI_SELF_TEST_PASSED ? "PASS"
                                                               : "FAIL";
  char fields[128];

  (void)snprintf(fields, sizeof(fields),
                 "state=%s address=0x%08lX size=1024", result,
                 (unsigned long)QSPI_TEST_START);
  return UartProtocol_SendLog(
      now_ms, status == QSPI_SELF_TEST_PASSED ? UART_PROTOCOL_LOG_INFO
                                                 : UART_PROTOCOL_LOG_ERROR,
      "qspi", "RW_TEST", fields);
}

static void FormatEncoderCount(char *buffer, size_t capacity, int64_t value)
{
  if (!UartProtocol_FormatSigned64(buffer, capacity, value)) {
    if (buffer != NULL && capacity > 1U) {
      buffer[0] = '0';
      buffer[1] = '\0';
    }
  }
}

static int32_t FixedFromFloat(float value, float scale)
{
  const float scaled = value * scale;

  if (!(scaled == scaled)) {
    return 0;
  }
  if (scaled >= 2147483647.0f) {
    return INT32_MAX;
  }
  if (scaled <= -2147483648.0f) {
    return INT32_MIN;
  }
  return (int32_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

static const char *TaskStateText(SystemStatusTaskState state)
{
  switch (state) {
    case SYSTEM_STATUS_TASK_RUNNING:
      return "RUNNING";
    case SYSTEM_STATUS_TASK_TIMEOUT:
      return "TIMEOUT";
    case SYSTEM_STATUS_TASK_NOT_STARTED:
    default:
      return "NOT_STARTED";
  }
}

static const char *ResetCauseText(uint32_t flags)
{
  if ((flags & RESET_CAUSE_IWDG) != 0U) {
    return "IWDG";
  }
  if ((flags & RESET_CAUSE_WWDG) != 0U) {
    return "WWDG";
  }
  if ((flags & RESET_CAUSE_SOFTWARE) != 0U) {
    return "SOFTWARE";
  }
  if ((flags & RESET_CAUSE_PIN) != 0U) {
    return "PIN";
  }
  if ((flags & RESET_CAUSE_BOR) != 0U) {
    return "BOR";
  }
  if ((flags & RESET_CAUSE_LOW_POWER) != 0U) {
    return "LOW_POWER";
  }
  if ((flags & RESET_CAUSE_OPTION_BYTE) != 0U) {
    return "OPTION_BYTE";
  }
  return "UNKNOWN";
}

static const char *ControlStateText(ChassisControlState state)
{
  switch (state) {
    case CHASSIS_CONTROL_RUNNING:
      return "RUNNING";
    case CHASSIS_CONTROL_COMMAND_TIMEOUT:
      return "COMMAND_TIMEOUT";
    case CHASSIS_CONTROL_EMERGENCY_STOP:
      return "EMERGENCY_STOP";
    case CHASSIS_CONTROL_INTERNAL_FAULT:
      return "INTERNAL_FAULT";
    case CHASSIS_CONTROL_OPEN_LOOP_TEST:
      return "OPEN_LOOP_TEST";
    case CHASSIS_CONTROL_STOPPED:
    default:
      return "STOPPED";
  }
}

static const char *CanStateText(SystemStatusCanState state)
{
  switch (state) {
    case SYSTEM_STATUS_CAN_PASSED:
      return "PASSED";
    case SYSTEM_STATUS_CAN_FAILED:
      return "FAILED";
    case SYSTEM_STATUS_CAN_READY:
    default:
      return "READY";
  }
}

static const char *LcdStateText(SystemStatusLcdState state)
{
  switch (state) {
    case SYSTEM_STATUS_LCD_DRAWING:
      return "DRAWING";
    case SYSTEM_STATUS_LCD_READY:
      return "READY";
    case SYSTEM_STATUS_LCD_FAILED:
      return "FAILED";
    case SYSTEM_STATUS_LCD_DISABLED:
    default:
      return "DISABLED";
  }
}

static const char *ImuStateText(const SystemStatusImuSnapshot *snapshot)
{
  if (snapshot == NULL) {
    return "UNINITIALIZED";
  }
  switch (snapshot->status) {
    case SYSTEM_STATUS_IMU_NOT_FOUND:
      return "NOT_FOUND";
    case SYSTEM_STATUS_IMU_READY:
      return snapshot->orientation_valid ? "READY" : "STARTING";
    case SYSTEM_STATUS_IMU_DEGRADED:
      return "DEGRADED";
    case SYSTEM_STATUS_IMU_UNINITIALIZED:
    default:
      return "UNINITIALIZED";
  }
}

static const char *OtaConfirmationText(uint32_t state)
{
  switch ((OtaConfirmationStatus)state) {
    case OTA_CONFIRMATION_NOT_REQUIRED:
      return "NOT_REQUIRED";
    case OTA_CONFIRMATION_RUNNING:
      return "RUNNING";
    case OTA_CONFIRMATION_CONFIRMED:
      return "CONFIRMED";
    case OTA_CONFIRMATION_FAILED:
      return "FAILED";
    case OTA_CONFIRMATION_WAITING:
    default:
      return "WAITING";
  }
}

static const char *TelemetryModeText(uint32_t mode)
{
  switch ((TelemetryMode)mode) {
    case TELEMETRY_MODE_TEXT:
      return "TEXT";
    case TELEMETRY_MODE_VOFA:
      return "VOFA";
    case TELEMETRY_MODE_OFF:
    default:
      return "OFF";
  }
}
