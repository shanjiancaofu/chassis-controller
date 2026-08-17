#include "infrastructure/console/diagnostic_report.h"

#include <stdio.h>

#include "bsp/uart/uart_bsp.h"
#include "bsp/reset/bsp_reset.h"
#include "communication/can_transport/can_transport.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "communication/ota_transport/ota_confirmation.h"
#include "communication/ota_transport/ota_session.h"
#include "communication/ota_transport/ota_uart_transport.h"
#include "components/imu_fusion/imu_fusion.h"
#include "config/storage_layout.h"
#include "infrastructure/console/console.h"
#include "infrastructure/telemetry/telemetry.h"
#include "modules/diagnostics/system_status.h"
#include "rtos/rtos_app.h"
#include "tests/target/iwdg_target_test.h"
#include "tests/target/qspi_target_test.h"

#define SELF_TEST_REPORT_DELAY_MS 100U

static uint32_t initialized_ms;
static bool initial_report_sent;
static bool self_test_report_requested;
static bool qspi_report_requested;
static bool iwdg_report_requested;
static bool motor_report_requested;
static char motor_report[96];
static char report_buffer[2048];

_Static_assert(sizeof(report_buffer) <= BSP_UART_MAX_WRITE_SIZE,
               "diagnostic report exceeds the UART write limit");

static bool WriteSelfTestReport(void);
static bool WriteQspiTestReport(void);
static const char *TaskStateText(uint32_t state);
static const char *ResetCauseText(uint32_t flags);

void DiagnosticReport_Init(uint32_t now_ms)
{
  initialized_ms = now_ms;
  initial_report_sent = false;
  self_test_report_requested = false;
  qspi_report_requested = false;
  iwdg_report_requested = false;
  motor_report_requested = false;
}

void DiagnosticReport_Run(uint32_t now_ms)
{
  static const char iwdg_armed[] =
      "IWDG_RESET_TEST: ARMED, reset expected within 10 seconds\r\n";

  if (QspiTargetTest_TakeCompletion()) {
    qspi_report_requested = true;
  }
  if ((!initial_report_sent &&
       (CanTransport_GetLinkStatus() != CAN_TRANSPORT_LINK_READY ||
        now_ms - initialized_ms >= SELF_TEST_REPORT_DELAY_MS)) ||
      self_test_report_requested) {
    if (WriteSelfTestReport()) {
      initial_report_sent = true;
      self_test_report_requested = false;
    }
    return;
  }
  if (qspi_report_requested && WriteQspiTestReport()) {
    qspi_report_requested = false;
    return;
  }
  if (iwdg_report_requested && BspUart_WriteString(iwdg_armed)) {
    iwdg_report_requested = false;
    return;
  }
  if (motor_report_requested && BspUart_WriteString(motor_report)) {
    motor_report_requested = false;
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

void DiagnosticReport_MotorTestResult(MotorTargetTestAction action,
                                      bool accepted)
{
  const char *action_text = "UNKNOWN";

  switch (action) {
    case MOTOR_TARGET_TEST_STOP:
      action_text = "STOP";
      break;
    case MOTOR_TARGET_TEST_LEFT_FORWARD:
      action_text = "LEFT FORWARD";
      break;
    case MOTOR_TARGET_TEST_LEFT_REVERSE:
      action_text = "LEFT REVERSE";
      break;
    case MOTOR_TARGET_TEST_RIGHT_FORWARD:
      action_text = "RIGHT FORWARD";
      break;
    case MOTOR_TARGET_TEST_RIGHT_REVERSE:
      action_text = "RIGHT REVERSE";
      break;
    default:
      return;
  }

  if (action == MOTOR_TARGET_TEST_STOP && accepted) {
    (void)snprintf(motor_report, sizeof(motor_report),
                   "MOTOR_TEST: STOPPED\r\n");
  } else {
    (void)snprintf(motor_report, sizeof(motor_report),
                   "MOTOR_TEST: %s %s\r\n",
                   accepted ? "STARTED" : "REJECTED", action_text);
  }
  motor_report_requested = true;
}

static bool WriteSelfTestReport(void)
{
  SystemStatusSnapshot system_status;
  QspiTargetTestStatus qspi_status;
  OtaConfirmationStatus ota_status;
  TelemetryMode telemetry_mode;
  bool rtc_ok;
  const char *fdcan_text = "READY";
  const char *lcd_text = "DISABLED";
  const char *qspi_rw_text = "DISABLED";
  const char *iwdg_text = "DISABLED";
  const char *imu_text = "NOT_INITIALIZED";
  const char *ota_text = "WAITING";
  char rtc_text[48];
  char qspi_text[96];
  size_t help_length;
  const char *help = Console_GetHelpText(&help_length);
  int length;
  (void)help_length;
  SystemStatus_GetSnapshot(&system_status);
  qspi_status = (QspiTargetTestStatus)system_status.qspi_test_state;
  ota_status =
      (OtaConfirmationStatus)system_status.ota_confirmation_state;
  telemetry_mode = (TelemetryMode)system_status.telemetry_mode;
  rtc_ok = system_status.rtc_valid;
  if (system_status.can_state == SYSTEM_STATUS_CAN_PASSED) {
    fdcan_text = "PASS";
  } else if (system_status.can_state == SYSTEM_STATUS_CAN_FAILED) {
    fdcan_text = "FAIL";
  }
  if (system_status.lcd_state == SYSTEM_STATUS_LCD_DRAWING) {
    lcd_text = "DRAWING";
  } else if (system_status.lcd_state == SYSTEM_STATUS_LCD_READY) {
    lcd_text = "READY";
  } else if (system_status.lcd_state == SYSTEM_STATUS_LCD_FAILED) {
    lcd_text = "FAIL";
  }
  if (qspi_status == QSPI_TARGET_TEST_RUNNING) {
    qspi_rw_text = "RUNNING";
  } else if (qspi_status == QSPI_TARGET_TEST_PASSED) {
    qspi_rw_text = "PASS";
  } else if (qspi_status == QSPI_TARGET_TEST_FAILED) {
    qspi_rw_text = "FAIL";
  }
  if (IwdgTargetTest_IsResetRequested()) {
    iwdg_text = "ARMED";
  } else if (system_status.board_health.iwdg_reset_test_passed) {
    iwdg_text = "PASS";
  }
  if (ota_status == OTA_CONFIRMATION_NOT_REQUIRED) {
    ota_text = "NOT_REQUIRED";
  } else if (ota_status == OTA_CONFIRMATION_RUNNING) {
    ota_text = "RUNNING";
  } else if (ota_status == OTA_CONFIRMATION_CONFIRMED) {
    ota_text = "CONFIRMED";
  } else if (ota_status == OTA_CONFIRMATION_FAILED) {
    ota_text = "FAIL";
  }
  if (system_status.imu.status == BSP_ICM45686_NOT_FOUND) {
    imu_text = "NOT_FOUND";
  } else if (system_status.imu.status == BSP_ICM45686_READY) {
    imu_text = system_status.imu.sample_valid ? "READY" : "STARTING";
  } else if (system_status.imu.status == BSP_ICM45686_DEGRADED) {
    imu_text = "DEGRADED";
  }

  if (rtc_ok) {
    (void)snprintf(rtc_text, sizeof(rtc_text),
                   "PASS time=20%02u-%02u-%02u %02u:%02u:%02u",
                   (unsigned int)system_status.rtc_year,
                   (unsigned int)system_status.rtc_month,
                   (unsigned int)system_status.rtc_date,
                   (unsigned int)system_status.rtc_hours,
                   (unsigned int)system_status.rtc_minutes,
                   (unsigned int)system_status.rtc_seconds);
  } else {
    (void)snprintf(rtc_text, sizeof(rtc_text), "FAIL");
  }
  if (!system_status.board_health.qspi_read_ok) {
    (void)snprintf(qspi_text, sizeof(qspi_text), "FAIL read");
  } else if (system_status.board_health.qspi_capacity_bytes == 0U) {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "FAIL jedec=%02X%02X%02X capacity=UNKNOWN",
                   system_status.board_health.qspi_jedec_id[0],
                   system_status.board_health.qspi_jedec_id[1],
                   system_status.board_health.qspi_jedec_id[2]);
  } else if (!system_status.board_health.qspi_id_valid) {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "FAIL jedec=%02X%02X%02X detected=%luMiB expected=%luMiB",
                   system_status.board_health.qspi_jedec_id[0],
                   system_status.board_health.qspi_jedec_id[1],
                   system_status.board_health.qspi_jedec_id[2],
                   (unsigned long)(system_status.board_health.qspi_capacity_bytes /
                                   (1024UL * 1024UL)),
                   (unsigned long)(QSPI_FLASH_CAPACITY_BYTES /
                                   (1024UL * 1024UL)));
  } else {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "PASS jedec=%02X%02X%02X capacity=%luMiB",
                   system_status.board_health.qspi_jedec_id[0],
                   system_status.board_health.qspi_jedec_id[1],
                   system_status.board_health.qspi_jedec_id[2],
                   (unsigned long)(system_status.board_health.qspi_capacity_bytes /
                                   (1024UL * 1024UL)));
  }

  length = snprintf(
      report_buffer, sizeof(report_buffer),
      "SELF_TEST\r\n"
      "USART_TX_DMA: READY\r\n"
      "USART_RX_DMA: READY\r\n"
      "RTC_READ: %s\r\n"
      "RTC_BACKUP: READY\r\n"
      "QSPI_ID: %s\r\n"
      "QSPI_RW_TEST: %s\r\n"
      "OTA_CONFIRM: %s\r\n"
      "OTA_TRANSFER: source=%u state=%u next=%lu uart_errors=%lu can_drops=%lu\r\n"
      "LCD: %s\r\n"
      "FDCAN_INTERNAL: DISABLED\r\n"
      "FDCAN_EXTERNAL: %s\r\n"
      "KEY: %s\r\n"
      "BUTTON_1: READY pressed=%u count=%lu\r\n"
      "BUTTON_2: READY pressed=%u count=%lu\r\n"
      "SENSORS:\r\n"
      "SR501: %s motion=%u raw=%u count=%lu last_ms=%lu warmup_ms=%lu\r\n"
      "ICM45686: %s whoami=0x%02X samples=%lu fifo=%lu irq=%lu errors=%lu parse=%lu full=%lu flush=%lu/%lu timestamp_errors=%lu dt_us=%lu dma_timeout=%lu raw_a=%d,%d,%d raw_g=%d,%d,%d\r\n"
      "IMU_FUSION: %s calibration=%u/%u bias_urad=%ld,%ld,%ld rpy_mrad=%ld,%ld,%ld yaw_unbounded=1\r\n"
      "MOTOR:\r\n"
      "CONTROL: state=%lu\r\n"
      "WHEEL: left[target=%ld speed=%ld pwm=%d] right[target=%ld speed=%ld pwm=%d]\r\n"
      "ENCODER: left_total=%lld right_total=%lld\r\n"
      "PID: left=%u,%u,%u right=%u,%u,%u\r\n"
      "MOTOR_TEST: running=%u left_pwm=%d right_pwm=%d\r\n"
      "CONTROL_OVERRUN: count=%lu missed=%lu\r\n"
      "SYSTEM:\r\n"
      "RTOS: uptime_ms=%lu critical=%u service[state=%s period_ms=%lu expected_ms=%lu timeout_ms=%lu age_ms=%lu stack_free_words=%lu runs=%lu] control[state=%s period_ms=%lu expected_ms=%lu timeout_ms=%lu age_ms=%lu stack_free_words=%lu runs=%lu] diagnostics[state=%s period_ms=%lu expected_ms=%lu timeout_ms=%lu age_ms=%lu stack_free_words=%lu runs=%lu] display[state=%s period_ms=%lu expected_ms=%lu timeout_ms=%lu age_ms=%lu stack_free_words=%lu runs=%lu]\r\n"
      "RESET: reason=%s flags=0x%08lX\r\n"
      "COMMUNICATION:\r\n"
      "IWDG_RESET_TEST: %s\r\n"
      "TELEMETRY: %s\r\n"
      "%s",
      rtc_text, qspi_text, qspi_rw_text, ota_text,
      (unsigned int)system_status.ota_source,
      (unsigned int)system_status.ota_state,
      (unsigned long)system_status.ota_next_offset,
      (unsigned long)system_status.uart_error_count,
      (unsigned long)system_status.can_drop_count, lcd_text, fdcan_text,
      system_status.board_health.button_test_passed ? "PASS" : "READY",
      system_status.buttons.pressed[BOARD_BUTTON_1] ? 1U : 0U,
      (unsigned long)system_status.buttons.pressed_count[BOARD_BUTTON_1],
      system_status.buttons.pressed[BOARD_BUTTON_2] ? 1U : 0U,
      (unsigned long)system_status.buttons.pressed_count[BOARD_BUTTON_2],
      system_status.sr501.status == BSP_SR501_READY ? "READY" : "WARMING_UP",
      system_status.sr501.motion_detected ? 1U : 0U,
      system_status.sr501.raw_high ? 1U : 0U,
      (unsigned long)system_status.sr501.event_count,
      (unsigned long)system_status.sr501.last_motion_ms,
      (unsigned long)system_status.sr501.warmup_remaining_ms,
      imu_text, system_status.imu.who_am_i,
      (unsigned long)system_status.imu.sample_count,
      (unsigned long)system_status.imu.fifo_frame_count,
      (unsigned long)system_status.imu.interrupt_count,
      (unsigned long)system_status.imu.transfer_error_count,
      (unsigned long)system_status.imu.fifo_parse_error_count,
      (unsigned long)system_status.imu.fifo_full_count,
      (unsigned long)system_status.imu.fifo_flush_count,
      (unsigned long)system_status.imu.fifo_flush_error_count,
      (unsigned long)system_status.imu.timestamp_error_count,
      (unsigned long)(system_status.imu.sample_period_s * 1000000.0f),
      (unsigned long)system_status.imu.dma_timeout_count,
      (int)system_status.imu.accel[0], (int)system_status.imu.accel[1],
      (int)system_status.imu.accel[2], (int)system_status.imu.gyro[0],
      (int)system_status.imu.gyro[1], (int)system_status.imu.gyro[2],
      system_status.imu.orientation_valid ? "READY"
                                           : system_status.imu.calibrated
                                                 ? "STARTING"
                                                 : "CALIBRATING",
      (unsigned int)system_status.imu.calibration_samples,
      (unsigned int)IMU_FUSION_CALIBRATION_SAMPLES,
      (long)(system_status.imu.gyro_bias_rad_s[0] * 1000000.0f),
      (long)(system_status.imu.gyro_bias_rad_s[1] * 1000000.0f),
      (long)(system_status.imu.gyro_bias_rad_s[2] * 1000000.0f),
      (long)(system_status.imu.roll_rad * 1000.0f),
      (long)(system_status.imu.pitch_rad * 1000.0f),
      (long)(system_status.imu.yaw_rad * 1000.0f),
      (unsigned long)system_status.control_state,
      (long)system_status.wheels.left_target,
      (long)system_status.wheels.left_measurement,
      (int)system_status.wheels.left_output,
      (long)system_status.wheels.right_target,
      (long)system_status.wheels.right_measurement,
      (int)system_status.wheels.right_output,
      (long long)system_status.odometry.left_total,
      (long long)system_status.odometry.right_total,
      (unsigned int)system_status.parameters.left_pid.kp,
      (unsigned int)system_status.parameters.left_pid.ki,
      (unsigned int)system_status.parameters.left_pid.kd,
      (unsigned int)system_status.parameters.right_pid.kp,
      (unsigned int)system_status.parameters.right_pid.ki,
      (unsigned int)system_status.parameters.right_pid.kd,
      system_status.motor_test.running ? 1U : 0U,
      (int)system_status.motor_test.left_duty,
      (int)system_status.motor_test.right_duty,
      (unsigned long)system_status.board_health.control_overrun_count,
      (unsigned long)system_status.board_health.control_missed_tick_count,
      (unsigned long)system_status.runtime.uptime_ms,
      system_status.runtime.critical_tasks_healthy ? 1U : 0U,
      TaskStateText(system_status.runtime.service_task_state),
      (unsigned long)system_status.runtime.service_period_ms,
      (unsigned long)system_status.runtime.service_expected_period_ms,
      (unsigned long)system_status.runtime.service_timeout_ms,
      (unsigned long)system_status.runtime.service_heartbeat_age_ms,
      (unsigned long)system_status.runtime.service_stack_high_water_words,
      (unsigned long)system_status.runtime.service_run_count,
      TaskStateText(system_status.runtime.control_task_state),
      (unsigned long)system_status.runtime.control_period_ms,
      (unsigned long)system_status.runtime.control_expected_period_ms,
      (unsigned long)system_status.runtime.control_timeout_ms,
      (unsigned long)system_status.runtime.control_heartbeat_age_ms,
      (unsigned long)system_status.runtime.control_stack_high_water_words,
      (unsigned long)system_status.runtime.control_run_count,
      TaskStateText(system_status.runtime.diagnostics_task_state),
      (unsigned long)system_status.runtime.diagnostics_period_ms,
      (unsigned long)system_status.runtime.diagnostics_expected_period_ms,
      (unsigned long)system_status.runtime.diagnostics_timeout_ms,
      (unsigned long)system_status.runtime.diagnostics_heartbeat_age_ms,
      (unsigned long)system_status.runtime.diagnostics_stack_high_water_words,
      (unsigned long)system_status.runtime.diagnostics_run_count,
      TaskStateText(system_status.runtime.display_task_state),
      (unsigned long)system_status.runtime.display_period_ms,
      (unsigned long)system_status.runtime.display_expected_period_ms,
      (unsigned long)system_status.runtime.display_timeout_ms,
      (unsigned long)system_status.runtime.display_heartbeat_age_ms,
      (unsigned long)system_status.runtime.display_stack_high_water_words,
      (unsigned long)system_status.runtime.display_run_count,
      ResetCauseText(system_status.board_health.reset_cause_flags),
      (unsigned long)system_status.board_health.reset_cause_flags,
      iwdg_text,
      telemetry_mode == TELEMETRY_MODE_TEXT
          ? "TEXT"
          : telemetry_mode == TELEMETRY_MODE_VOFA ? "VOFA" : "OFF",
      help);
  return length > 0 && (size_t)length < sizeof(report_buffer) &&
         BspUart_Write(report_buffer, (size_t)length);
}

static const char *TaskStateText(uint32_t state)
{
  switch ((RtosAppTaskState)state) {
    case RTOS_APP_TASK_RUNNING:
      return "RUNNING";
    case RTOS_APP_TASK_TIMEOUT:
      return "TIMEOUT";
    case RTOS_APP_TASK_NOT_STARTED:
    default:
      return "NOT_STARTED";
  }
}

static const char *ResetCauseText(uint32_t flags)
{
  if ((flags & BSP_RESET_CAUSE_IWDG) != 0U) {
    return "IWDG";
  }
  if ((flags & BSP_RESET_CAUSE_WWDG) != 0U) {
    return "WWDG";
  }
  if ((flags & BSP_RESET_CAUSE_SOFTWARE) != 0U) {
    return "SOFTWARE";
  }
  if ((flags & BSP_RESET_CAUSE_PIN) != 0U) {
    return "PIN";
  }
  if ((flags & BSP_RESET_CAUSE_BOR) != 0U) {
    return "BOR";
  }
  if ((flags & BSP_RESET_CAUSE_LOW_POWER) != 0U) {
    return "LOW_POWER";
  }
  if ((flags & BSP_RESET_CAUSE_OPTION_BYTE) != 0U) {
    return "OPTION_BYTE";
  }
  return "UNKNOWN";
}

static bool WriteQspiTestReport(void)
{
  const QspiTargetTestStatus status = QspiTargetTest_GetStatus();
  const char *result = status == QSPI_TARGET_TEST_RUNNING
                           ? "RUNNING"
                           : status == QSPI_TARGET_TEST_PASSED ? "PASS"
                                                               : "FAIL";
  const int length = snprintf(
      report_buffer, sizeof(report_buffer),
      "QSPI_RW_TEST: %s address=0x%08lX size=%u DMA\r\n", result,
      (unsigned long)QSPI_TEST_START, 1024U);

  return length > 0 && (size_t)length < sizeof(report_buffer) &&
         BspUart_Write(report_buffer, (size_t)length);
}
