#include "modules/diagnostics/board_self_test.h"

#include <stdio.h>
#include <string.h>

#include "bsp/lcd/bsp_lcd.h"
#include "bsp/qspi/bsp_qspi_flash.h"
#include "bsp/uart/uart_bsp.h"
#include "communication/can_transport/can_transport.h"
#include "config/storage_layout.h"
#include "infrastructure/console/console.h"
#include "infrastructure/telemetry/telemetry.h"
#include "quadspi.h"
#include "rtc.h"

#define SELF_TEST_REPORT_DELAY_MS 100U
#define QSPI_JEDEC_ID_COMMAND 0x9FU
#define QSPI_COMMAND_TIMEOUT_MS 100U
#define QSPI_ERASE_TIMEOUT_MS 5000U
#define QSPI_PROGRAM_TIMEOUT_MS 500U
#define QSPI_DMA_TIMEOUT_MS 500U
#define QSPI_TEST_DATA_SIZE 1024U
#define IWDG_TEST_MARKER 0x49574447UL

typedef enum {
  QSPI_TEST_IDLE = 0,
  QSPI_TEST_ERASE_START,
  QSPI_TEST_ERASE_WAIT,
  QSPI_TEST_ERASE_READ_START,
  QSPI_TEST_ERASE_READ_WAIT,
  QSPI_TEST_ERASE_VERIFY,
  QSPI_TEST_PROGRAM_START,
  QSPI_TEST_PROGRAM_DMA_WAIT,
  QSPI_TEST_PROGRAM_FLASH_WAIT,
  QSPI_TEST_READ_START,
  QSPI_TEST_READ_WAIT,
  QSPI_TEST_VERIFY,
  QSPI_TEST_PASSED,
  QSPI_TEST_FAILED
} QspiTestState;

static RTC_TimeTypeDef rtc_time;
static RTC_DateTypeDef rtc_date;
static bool rtc_read_ok;
static uint8_t qspi_jedec_id[3];
static uint32_t qspi_capacity_bytes;
static bool qspi_read_ok;
static QspiTestState qspi_test_state;
static uint32_t qspi_test_deadline_ms;
static uint32_t qspi_test_offset;
static bool qspi_test_report_requested;
static uint8_t qspi_test_read_buffer[QSPI_TEST_DATA_SIZE];
static uint8_t qspi_test_pattern[QSPI_TEST_DATA_SIZE];
static bool initial_report_sent;
static bool report_requested;
static bool button_test_passed;
static bool iwdg_reset_test_passed;
static bool iwdg_reset_requested;
static bool iwdg_reset_report_requested;
static BoardMotorTestRequest motor_test_request;
static bool motor_test_report_requested;
static uint32_t self_test_started_ms;
static char self_test_report[1152];
static char qspi_test_report[128];
static char motor_test_report[96];

static void QspiRunTest(uint32_t now_ms);
static bool QspiTestIsRunning(void);
static bool WriteSelfTestReport(void);
static bool WriteQspiTestReport(void);

bool BoardSelfTest_Init(void)
{
  QSPI_CommandTypeDef qspi_command = {0};
  const bool iwdg_reset_flag =
      __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET;
  const bool iwdg_test_marker =
      HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2) == IWDG_TEST_MARKER;
  size_t index;

  self_test_started_ms = HAL_GetTick();
  qspi_test_state = QSPI_TEST_IDLE;
  for (index = 0U; index < sizeof(qspi_test_pattern); ++index) {
    qspi_test_pattern[index] = (uint8_t)((index * 37U + 0x5AU) & 0xFFU);
  }

  iwdg_reset_test_passed = iwdg_reset_flag && iwdg_test_marker;
  if (iwdg_test_marker) {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, 0U);
  }
  __HAL_RCC_CLEAR_RESET_FLAGS();

  qspi_command.Instruction = QSPI_JEDEC_ID_COMMAND;
  qspi_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  qspi_command.AddressMode = QSPI_ADDRESS_NONE;
  qspi_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  qspi_command.DataMode = QSPI_DATA_1_LINE;
  qspi_command.DummyCycles = 0U;
  qspi_command.NbData = sizeof(qspi_jedec_id);
  qspi_command.DdrMode = QSPI_DDR_MODE_DISABLE;
  qspi_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  qspi_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  qspi_read_ok =
      HAL_QSPI_Command(&hqspi1, &qspi_command, QSPI_COMMAND_TIMEOUT_MS) ==
          HAL_OK &&
      HAL_QSPI_Receive(&hqspi1, qspi_jedec_id, QSPI_COMMAND_TIMEOUT_MS) ==
          HAL_OK;
  if (qspi_read_ok && qspi_jedec_id[2] >= 0x10U &&
      qspi_jedec_id[2] <= 0x1FU) {
    qspi_capacity_bytes = 1UL << qspi_jedec_id[2];
  }
  return true;
}

void BoardSelfTest_Run(uint32_t now_ms)
{
  static const char iwdg_armed[] =
      "IWDG_RESET_TEST: ARMED, reset expected within 2 seconds\r\n";

  QspiRunTest(now_ms);

  if ((!initial_report_sent &&
       (CanTransport_GetLinkStatus() != CAN_TRANSPORT_LINK_READY ||
        now_ms - self_test_started_ms >= SELF_TEST_REPORT_DELAY_MS)) ||
      report_requested) {
    if (WriteSelfTestReport()) {
      initial_report_sent = true;
      report_requested = false;
    }
    return;
  }

  if (qspi_test_report_requested && WriteQspiTestReport()) {
    qspi_test_report_requested = false;
    return;
  }

  if (iwdg_reset_report_requested && BspUart_WriteString(iwdg_armed)) {
    iwdg_reset_report_requested = false;
    return;
  }

  if (motor_test_report_requested && BspUart_WriteString(motor_test_report)) {
    motor_test_report_requested = false;
  }
}

void BoardSelfTest_GetStatus(BoardSelfTestStatus *status)
{
  if (status == NULL) {
    return;
  }
  status->qspi_id_valid =
      qspi_read_ok && qspi_capacity_bytes == 8UL * 1024UL * 1024UL;
  status->qspi_capacity_bytes = qspi_capacity_bytes;
  status->iwdg_reset_test_passed = iwdg_reset_test_passed;
}

void BoardSelfTest_RequestReport(void)
{
  report_requested = true;
}

bool BoardSelfTest_RequestQspiTest(void)
{
  if (iwdg_reset_requested || QspiTestIsRunning()) {
    return false;
  }

  qspi_test_state =
      qspi_read_ok && qspi_capacity_bytes == 8UL * 1024UL * 1024UL
          ? QSPI_TEST_ERASE_START
          : QSPI_TEST_FAILED;
  qspi_test_report_requested = true;
  return true;
}

bool BoardSelfTest_RequestIwdgReset(void)
{
  if (QspiTestIsRunning()) {
    return false;
  }

  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, IWDG_TEST_MARKER);
  iwdg_reset_requested = true;
  iwdg_reset_report_requested = true;
  return true;
}

bool BoardSelfTest_RequestMotorTest(BoardMotorTestRequest request)
{
  if (request == BOARD_MOTOR_TEST_NONE) {
    return false;
  }
  if (request != BOARD_MOTOR_TEST_STOP &&
      (motor_test_request != BOARD_MOTOR_TEST_NONE ||
       iwdg_reset_requested || QspiTestIsRunning())) {
    return false;
  }

  motor_test_request = request;
  return true;
}

void BoardSelfTest_NotifyButtonPressed(void)
{
  button_test_passed = true;
  report_requested = true;
}

bool BoardSelfTest_IsIwdgResetRequested(void)
{
  return iwdg_reset_requested;
}

BoardMotorTestRequest BoardSelfTest_TakeMotorTestRequest(void)
{
  const BoardMotorTestRequest request = motor_test_request;

  motor_test_request = BOARD_MOTOR_TEST_NONE;
  return request;
}

void BoardSelfTest_ReportMotorTestResult(BoardMotorTestRequest request,
                                         bool accepted)
{
  const char *action = "UNKNOWN";

  switch (request) {
    case BOARD_MOTOR_TEST_STOP:
      action = "STOP";
      break;
    case BOARD_MOTOR_TEST_LEFT_FORWARD:
      action = "LEFT FORWARD";
      break;
    case BOARD_MOTOR_TEST_LEFT_REVERSE:
      action = "LEFT REVERSE";
      break;
    case BOARD_MOTOR_TEST_RIGHT_FORWARD:
      action = "RIGHT FORWARD";
      break;
    case BOARD_MOTOR_TEST_RIGHT_REVERSE:
      action = "RIGHT REVERSE";
      break;
    default:
      return;
  }

  if (request == BOARD_MOTOR_TEST_STOP && accepted) {
    (void)snprintf(motor_test_report, sizeof(motor_test_report),
                   "MOTOR_TEST: STOPPED\r\n");
  } else {
    (void)snprintf(motor_test_report, sizeof(motor_test_report),
                   "MOTOR_TEST: %s %s\r\n",
                   accepted ? "STARTED" : "REJECTED", action);
  }
  motor_test_report_requested = true;
}

static void QspiRunTest(uint32_t now_ms)
{
  BspQspiTransferStatus transfer_status;
  bool flash_busy;
  size_t index;

  switch (qspi_test_state) {
    case QSPI_TEST_ERASE_START:
      if (BspQspiFlash_EraseSector(QSPI_TEST_START)) {
        qspi_test_deadline_ms = now_ms + QSPI_ERASE_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_ERASE_WAIT;
      } else {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_ERASE_WAIT:
      if (!BspQspiFlash_IsBusy(&flash_busy)) {
        qspi_test_state = QSPI_TEST_FAILED;
      } else if (!flash_busy) {
        qspi_test_state = QSPI_TEST_ERASE_READ_START;
      } else if ((int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_ERASE_READ_START:
      if (BspQspiFlash_ReadDma(QSPI_TEST_START, qspi_test_read_buffer,
                               sizeof(qspi_test_read_buffer))) {
        qspi_test_deadline_ms = now_ms + QSPI_DMA_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_ERASE_READ_WAIT;
      } else {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_ERASE_READ_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        qspi_test_state = QSPI_TEST_ERASE_VERIFY;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 (int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        (void)HAL_QSPI_Abort(&hqspi1);
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_ERASE_VERIFY:
      for (index = 0U; index < sizeof(qspi_test_read_buffer); ++index) {
        if (qspi_test_read_buffer[index] != 0xFFU) {
          qspi_test_state = QSPI_TEST_FAILED;
          break;
        }
      }
      if (qspi_test_state == QSPI_TEST_ERASE_VERIFY) {
        qspi_test_offset = 0U;
        qspi_test_state = QSPI_TEST_PROGRAM_START;
      }
      break;
    case QSPI_TEST_PROGRAM_START:
      if (BspQspiFlash_ProgramPageDma(
              QSPI_TEST_START + qspi_test_offset,
              &qspi_test_pattern[qspi_test_offset], QSPI_FLASH_PAGE_SIZE)) {
        qspi_test_deadline_ms = now_ms + QSPI_DMA_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_PROGRAM_DMA_WAIT;
      } else {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_PROGRAM_DMA_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        qspi_test_deadline_ms = now_ms + QSPI_PROGRAM_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_PROGRAM_FLASH_WAIT;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 (int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        (void)HAL_QSPI_Abort(&hqspi1);
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_PROGRAM_FLASH_WAIT:
      if (!BspQspiFlash_IsBusy(&flash_busy)) {
        qspi_test_state = QSPI_TEST_FAILED;
      } else if (!flash_busy) {
        qspi_test_offset += QSPI_FLASH_PAGE_SIZE;
        qspi_test_state = qspi_test_offset < QSPI_TEST_DATA_SIZE
                              ? QSPI_TEST_PROGRAM_START
                              : QSPI_TEST_READ_START;
      } else if ((int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_READ_START:
      if (BspQspiFlash_ReadDma(QSPI_TEST_START, qspi_test_read_buffer,
                               sizeof(qspi_test_read_buffer))) {
        qspi_test_deadline_ms = now_ms + QSPI_DMA_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_READ_WAIT;
      } else {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_READ_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        qspi_test_state = QSPI_TEST_VERIFY;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 (int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        (void)HAL_QSPI_Abort(&hqspi1);
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_VERIFY:
      qspi_test_state =
          memcmp(qspi_test_read_buffer, qspi_test_pattern,
                 sizeof(qspi_test_pattern)) == 0
              ? QSPI_TEST_PASSED
              : QSPI_TEST_FAILED;
      break;
    default:
      return;
  }

  if (qspi_test_state == QSPI_TEST_PASSED ||
      qspi_test_state == QSPI_TEST_FAILED) {
    qspi_test_report_requested = true;
  }
}

static bool QspiTestIsRunning(void)
{
  return qspi_test_state >= QSPI_TEST_ERASE_START &&
         qspi_test_state <= QSPI_TEST_VERIFY;
}

static bool WriteSelfTestReport(void)
{
  const CanTransportLinkStatus can_status = CanTransport_GetLinkStatus();
  const TelemetryMode telemetry_mode = Telemetry_GetMode();
  const char *fdcan_text = "READY";
  const char *lcd_text = "DISABLED";
  const char *qspi_rw_text = "DISABLED";
  const char *iwdg_text = "DISABLED";
  const uint32_t configured_qspi_capacity =
      1UL << (hqspi1.Init.FlashSize + 1U);
  char rtc_text[48];
  char qspi_text[96];
  size_t help_length;
  const char *help = Console_GetHelpText(&help_length);
  int length;

  (void)help_length;
  rtc_read_ok =
      HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN) == HAL_OK &&
      HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN) == HAL_OK &&
      rtc_time.Hours <= 23U && rtc_time.Minutes <= 59U &&
      rtc_time.Seconds <= 59U && rtc_date.Month >= 1U &&
      rtc_date.Month <= 12U && rtc_date.Date >= 1U && rtc_date.Date <= 31U;

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

  if (QspiTestIsRunning()) {
    qspi_rw_text = "RUNNING";
  } else if (qspi_test_state == QSPI_TEST_PASSED) {
    qspi_rw_text = "PASS";
  } else if (qspi_test_state == QSPI_TEST_FAILED) {
    qspi_rw_text = "FAIL";
  }
  if (iwdg_reset_requested) {
    iwdg_text = "ARMED";
  } else if (iwdg_reset_test_passed) {
    iwdg_text = "PASS";
  }

  if (rtc_read_ok) {
    (void)snprintf(rtc_text, sizeof(rtc_text),
                   "PASS time=20%02u-%02u-%02u %02u:%02u:%02u",
                   (unsigned int)rtc_date.Year,
                   (unsigned int)rtc_date.Month,
                   (unsigned int)rtc_date.Date,
                   (unsigned int)rtc_time.Hours,
                   (unsigned int)rtc_time.Minutes,
                   (unsigned int)rtc_time.Seconds);
  } else {
    (void)snprintf(rtc_text, sizeof(rtc_text), "FAIL");
  }

  if (!qspi_read_ok) {
    (void)snprintf(qspi_text, sizeof(qspi_text), "FAIL read");
  } else if (qspi_capacity_bytes == 0U) {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "FAIL jedec=%02X%02X%02X capacity=UNKNOWN",
                   qspi_jedec_id[0], qspi_jedec_id[1], qspi_jedec_id[2]);
  } else if (qspi_capacity_bytes != configured_qspi_capacity) {
    (void)snprintf(
        qspi_text, sizeof(qspi_text),
        "FAIL jedec=%02X%02X%02X detected=%luMiB configured=%luMiB",
        qspi_jedec_id[0], qspi_jedec_id[1], qspi_jedec_id[2],
        (unsigned long)(qspi_capacity_bytes / (1024UL * 1024UL)),
        (unsigned long)(configured_qspi_capacity / (1024UL * 1024UL)));
  } else {
    (void)snprintf(qspi_text, sizeof(qspi_text),
                   "PASS jedec=%02X%02X%02X capacity=%luMiB",
                   qspi_jedec_id[0], qspi_jedec_id[1], qspi_jedec_id[2],
                   (unsigned long)(qspi_capacity_bytes / (1024UL * 1024UL)));
  }

  length = snprintf(
      self_test_report, sizeof(self_test_report),
      "SELF_TEST\r\n"
      "USART_TX_DMA: READY\r\n"
      "USART_RX_DMA: READY\r\n"
      "RTC_READ: %s\r\n"
      "RTC_BACKUP: READY\r\n"
      "QSPI_ID: %s\r\n"
      "QSPI_RW_TEST: %s\r\n"
      "LCD: %s\r\n"
      "FDCAN_INTERNAL: DISABLED\r\n"
      "FDCAN_EXTERNAL: %s\r\n"
      "KEY: %s\r\n"
      "ENCODER: READY\r\n"
      "MOTOR: DISABLED\r\n"
      "IWDG_RESET_TEST: %s\r\n"
      "TELEMETRY: %s\r\n"
      "%s",
      rtc_text, qspi_text, qspi_rw_text, lcd_text, fdcan_text,
      button_test_passed ? "PASS" : "READY", iwdg_text,
      telemetry_mode == TELEMETRY_MODE_TEXT
          ? "TEXT"
          : telemetry_mode == TELEMETRY_MODE_VOFA ? "VOFA" : "OFF",
      help);
  return length > 0 && (size_t)length < sizeof(self_test_report) &&
         BspUart_Write(self_test_report, (size_t)length);
}

static bool WriteQspiTestReport(void)
{
  const bool running = QspiTestIsRunning();
  const char *result = running       ? "RUNNING"
                       : qspi_test_state == QSPI_TEST_PASSED ? "PASS"
                                                             : "FAIL";
  const int length = snprintf(
      qspi_test_report, sizeof(qspi_test_report),
      "QSPI_RW_TEST: %s address=0x%08lX size=%u DMA\r\n", result,
      (unsigned long)QSPI_TEST_START, (unsigned int)QSPI_TEST_DATA_SIZE);

  return length > 0 && (size_t)length < sizeof(qspi_test_report) &&
         BspUart_Write(qspi_test_report, (size_t)length);
}
