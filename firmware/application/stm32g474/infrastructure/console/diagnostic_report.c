#include "infrastructure/console/diagnostic_report.h"

#include <stdio.h>

#include "bsp/button/bsp_button.h"
#include "bsp/lcd/bsp_lcd.h"
#include "bsp/imu/bsp_icm45686.h"
#include "bsp/uart/uart_bsp.h"
#include "communication/can_transport/can_transport.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "communication/ota_transport/ota_confirmation.h"
#include "communication/ota_transport/ota_session.h"
#include "communication/ota_transport/ota_uart_transport.h"
#include "config/storage_layout.h"
#include "infrastructure/console/console.h"
#include "infrastructure/telemetry/telemetry.h"
#include "modules/diagnostics/board_health.h"
#include "rtc.h"
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
static char report_buffer[1280];

static bool WriteSelfTestReport(void);
static bool WriteQspiTestReport(void);

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
  BoardHealthSnapshot health;
  BspButtonSnapshot buttons;
  BspIcm45686Snapshot imu;
  RTC_TimeTypeDef time;
  RTC_DateTypeDef date;
  const CanTransportLinkStatus can_status = CanTransport_GetLinkStatus();
  const TelemetryMode telemetry_mode = Telemetry_GetMode();
  const QspiTargetTestStatus qspi_status = QspiTargetTest_GetStatus();
  const OtaConfirmationStatus ota_status = OtaConfirmation_GetStatus();
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
  const bool rtc_ok =
      HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK &&
      HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK &&
      time.Hours <= 23U && time.Minutes <= 59U && time.Seconds <= 59U &&
      date.Month >= 1U && date.Month <= 12U &&
      date.Date >= 1U && date.Date <= 31U;

  (void)help_length;
  BoardHealth_GetSnapshot(&health);
  BspButton_GetSnapshot(&buttons);
  BspIcm45686_GetSnapshot(&imu);
  if (can_status == CAN_TRANSPORT_LINK_PASSED) {
    fdcan_text = "PASS";
  } else if (can_status == CAN_TRANSPORT_LINK_FAILED) {
    fdcan_text = "FAIL";
  }
  if (BspLcd_GetStatus() == BSP_LCD_DRAWING) {
    lcd_text = "DRAWING";
  } else if (BspLcd_GetStatus() == BSP_LCD_READY) {
    lcd_text = "READY";
  } else if (BspLcd_GetStatus() == BSP_LCD_FAILED) {
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
  } else if (health.iwdg_reset_test_passed) {
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
  if (imu.status == BSP_ICM45686_NOT_FOUND) {
    imu_text = "NOT_FOUND";
  } else if (imu.status == BSP_ICM45686_READY) {
    imu_text = imu.sample_valid ? "READY" : "STARTING";
  } else if (imu.status == BSP_ICM45686_DEGRADED) {
    imu_text = "DEGRADED";
  }

  if (rtc_ok) {
    (void)snprintf(rtc_text, sizeof(rtc_text),
                   "PASS time=20%02u-%02u-%02u %02u:%02u:%02u",
                   (unsigned int)date.Year, (unsigned int)date.Month,
                   (unsigned int)date.Date, (unsigned int)time.Hours,
                   (unsigned int)time.Minutes, (unsigned int)time.Seconds);
  } else {
    (void)snprintf(rtc_text, sizeof(rtc_text), "FAIL");
  }
  if (!health.qspi_read_ok) {
    (void)snprintf(qspi_text, sizeof(qspi_text), "FAIL read");
  } else if (health.qspi_capacity_bytes == 0U) {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "FAIL jedec=%02X%02X%02X capacity=UNKNOWN",
                   health.qspi_jedec_id[0], health.qspi_jedec_id[1],
                   health.qspi_jedec_id[2]);
  } else if (!health.qspi_id_valid) {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "FAIL jedec=%02X%02X%02X detected=%luMiB expected=%luMiB",
                   health.qspi_jedec_id[0], health.qspi_jedec_id[1],
                   health.qspi_jedec_id[2],
                   (unsigned long)(health.qspi_capacity_bytes /
                                   (1024UL * 1024UL)),
                   (unsigned long)(QSPI_FLASH_CAPACITY_BYTES /
                                   (1024UL * 1024UL)));
  } else {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "PASS jedec=%02X%02X%02X capacity=%luMiB",
                   health.qspi_jedec_id[0], health.qspi_jedec_id[1],
                   health.qspi_jedec_id[2],
                   (unsigned long)(health.qspi_capacity_bytes /
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
      "ICM45686: %s whoami=0x%02X samples=%lu irq=%lu errors=%lu raw_a=%d,%d,%d raw_g=%d,%d,%d\r\n"
      "ENCODER: READY\r\n"
      "MOTOR: DISABLED\r\n"
      "CONTROL_OVERRUN: count=%lu missed=%lu\r\n"
      "IWDG_RESET_TEST: %s\r\n"
      "TELEMETRY: %s\r\n"
      "%s",
      rtc_text, qspi_text, qspi_rw_text, ota_text,
      (unsigned int)OtaSession_GetSource(),
      (unsigned int)OtaSession_GetState(),
      (unsigned long)OtaSession_GetNextOffset(),
      (unsigned long)OtaUartTransport_GetErrorCount(),
      (unsigned long)OtaCanTransport_GetDroppedCount(), lcd_text, fdcan_text,
      health.button_test_passed ? "PASS" : "READY",
      buttons.pressed[BOARD_BUTTON_1] ? 1U : 0U,
      (unsigned long)buttons.pressed_count[BOARD_BUTTON_1],
      buttons.pressed[BOARD_BUTTON_2] ? 1U : 0U,
      (unsigned long)buttons.pressed_count[BOARD_BUTTON_2],
      imu_text, imu.who_am_i, (unsigned long)imu.sample_count,
      (unsigned long)imu.interrupt_count,
      (unsigned long)imu.transfer_error_count,
      (int)imu.accel[0], (int)imu.accel[1], (int)imu.accel[2],
      (int)imu.gyro[0], (int)imu.gyro[1], (int)imu.gyro[2],
      (unsigned long)health.control_overrun_count,
      (unsigned long)health.control_missed_tick_count, iwdg_text,
      telemetry_mode == TELEMETRY_MODE_TEXT
          ? "TEXT"
          : telemetry_mode == TELEMETRY_MODE_VOFA ? "VOFA" : "OFF",
      help);
  return length > 0 && (size_t)length < sizeof(report_buffer) &&
         BspUart_Write(report_buffer, (size_t)length);
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
