#include "board_self_test.h"

#include <stdio.h>
#include <string.h>

#include "bsp_lcd.h"
#include "bsp_qspi_flash.h"
#include "fdcan_driver.h"
#include "main.h"
#include "quadspi.h"
#include "rtc.h"
#include "storage_layout.h"
#include "usart.h"

#define UART_RX_DMA_BUFFER_SIZE 128U
#define UART_COMMAND_BUFFER_SIZE 32U
#define SELF_TEST_REPORT_DELAY_MS 100U
#define QSPI_JEDEC_ID_COMMAND 0x9FU
#define QSPI_COMMAND_TIMEOUT_MS 100U
#define QSPI_ERASE_TIMEOUT_MS 5000U
#define QSPI_PROGRAM_TIMEOUT_MS 500U
#define QSPI_DMA_TIMEOUT_MS 500U
#define QSPI_TEST_DATA_SIZE 1024U
#define IWDG_TEST_MARKER 0x49574447UL

typedef enum
{
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

static uint8_t uart_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];
static volatile uint16_t uart_rx_dma_position;
static volatile bool uart_rx_event_pending;
static uint16_t uart_rx_read_position;
static char uart_command[UART_COMMAND_BUFFER_SIZE];
static uint8_t uart_command_length;

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
static bool pong_requested;
static bool help_requested;
static bool telemetry_enabled;
static bool telemetry_state_requested;
static bool button_test_passed;
static bool iwdg_reset_test_passed;
static bool iwdg_reset_requested;
static bool iwdg_reset_report_requested;
static BoardMotorTestRequest motor_test_request;
static bool motor_test_report_requested;
static uint32_t self_test_started_ms;
static char self_test_report[768];
static char qspi_test_report[128];
static char motor_test_report[96];

static void QspiRunTest(uint32_t now_ms)
{
  BspQspiTransferStatus transfer_status;
  bool flash_busy;

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
      if (BspQspiFlash_ReadDma(QSPI_TEST_START,
                               qspi_test_read_buffer,
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
      for (size_t index = 0U; index < sizeof(qspi_test_read_buffer);
           ++index) {
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
              &qspi_test_pattern[qspi_test_offset],
              QSPI_FLASH_PAGE_SIZE)) {
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
      if (BspQspiFlash_ReadDma(QSPI_TEST_START,
                               qspi_test_read_buffer,
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

bool BoardSelfTest_Init(void)
{
  QSPI_CommandTypeDef qspi_command = {0};
  const bool iwdg_reset_flag =
      __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET;
  const bool iwdg_test_marker =
      HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2) == IWDG_TEST_MARKER;

  self_test_started_ms = HAL_GetTick();
  telemetry_enabled = false;
  qspi_test_state = QSPI_TEST_IDLE;
  for (size_t index = 0U; index < sizeof(qspi_test_pattern); ++index) {
    qspi_test_pattern[index] =
        (uint8_t)((index * 37U + 0x5AU) & 0xFFU);
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

  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer,
                                   sizeof(uart_rx_dma_buffer)) != HAL_OK) {
    return false;
  }
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  return true;
}

bool BoardSelfTest_Run(uint32_t now_ms)
{
  static const uint8_t pong[] = "PONG\r\n";
  static const uint8_t telemetry_on[] = "TELEMETRY: ON\r\n";
  static const uint8_t telemetry_off[] = "TELEMETRY: OFF\r\n";
  static const uint8_t help[] =
      "COMMANDS: ping, status, telemetry on, telemetry off, qspi test confirm, iwdg reset confirm, motor left forward confirm, motor left reverse confirm, motor right forward confirm, motor right reverse confirm, motor stop\r\n";
  static const uint8_t iwdg_armed[] =
      "IWDG_RESET_TEST: ARMED, reset expected within 2 seconds\r\n";
  uint16_t dma_position = 0U;
  bool process_rx = false;
  uint32_t primask = __get_PRIMASK();
  const FdcanLoopbackStatus fdcan_status =
      FdcanDriver_GetLoopbackStatus();

  __disable_irq();
  if (uart_rx_event_pending) {
    dma_position = uart_rx_dma_position;
    uart_rx_event_pending = false;
    process_rx = true;
  }
  __set_PRIMASK(primask);

  if (process_rx) {
    uint16_t bytes_available;

    if (dma_position == UART_RX_DMA_BUFFER_SIZE) {
      bytes_available =
          UART_RX_DMA_BUFFER_SIZE - uart_rx_read_position;
    } else if (dma_position >= uart_rx_read_position) {
      bytes_available = dma_position - uart_rx_read_position;
    } else {
      bytes_available =
          UART_RX_DMA_BUFFER_SIZE - uart_rx_read_position + dma_position;
    }

    while (bytes_available > 0U) {
      const uint8_t value = uart_rx_dma_buffer[uart_rx_read_position];

      uart_rx_read_position =
          (uint16_t)((uart_rx_read_position + 1U) %
                     UART_RX_DMA_BUFFER_SIZE);
      --bytes_available;
      if (value == '\n') {
        BoardMotorTestRequest requested_motor_test =
            BOARD_MOTOR_TEST_NONE;

        uart_command[uart_command_length] = '\0';
        if (strcmp(uart_command, "ping") == 0) {
          pong_requested = true;
        } else if (strcmp(uart_command, "status") == 0) {
          report_requested = true;
        } else if (strcmp(uart_command, "telemetry on") == 0) {
          telemetry_enabled = true;
          telemetry_state_requested = true;
        } else if (strcmp(uart_command, "telemetry off") == 0) {
          telemetry_enabled = false;
          telemetry_state_requested = true;
        } else if (strcmp(uart_command, "qspi test confirm") == 0) {
          if (!iwdg_reset_requested &&
              (qspi_test_state == QSPI_TEST_IDLE ||
               qspi_test_state == QSPI_TEST_PASSED ||
               qspi_test_state == QSPI_TEST_FAILED)) {
            qspi_test_state =
                qspi_read_ok &&
                        qspi_capacity_bytes == 8UL * 1024UL * 1024UL
                    ? QSPI_TEST_ERASE_START
                    : QSPI_TEST_FAILED;
          }
          qspi_test_report_requested = true;
        } else if (strcmp(uart_command, "iwdg reset confirm") == 0) {
          if (!(qspi_test_state >= QSPI_TEST_ERASE_START &&
                qspi_test_state <= QSPI_TEST_VERIFY)) {
            HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2,
                                IWDG_TEST_MARKER);
            iwdg_reset_requested = true;
            iwdg_reset_report_requested = true;
          } else {
            help_requested = true;
          }
        } else if (strcmp(uart_command, "motor stop") == 0) {
          requested_motor_test = BOARD_MOTOR_TEST_STOP;
        } else if (strcmp(uart_command,
                          "motor left forward confirm") == 0) {
          requested_motor_test = BOARD_MOTOR_TEST_LEFT_FORWARD;
        } else if (strcmp(uart_command,
                          "motor left reverse confirm") == 0) {
          requested_motor_test = BOARD_MOTOR_TEST_LEFT_REVERSE;
        } else if (strcmp(uart_command,
                          "motor right forward confirm") == 0) {
          requested_motor_test = BOARD_MOTOR_TEST_RIGHT_FORWARD;
        } else if (strcmp(uart_command,
                          "motor right reverse confirm") == 0) {
          requested_motor_test = BOARD_MOTOR_TEST_RIGHT_REVERSE;
        } else if (uart_command_length != 0U) {
          help_requested = true;
        }
        if (requested_motor_test == BOARD_MOTOR_TEST_STOP ||
            (requested_motor_test != BOARD_MOTOR_TEST_NONE &&
             motor_test_request == BOARD_MOTOR_TEST_NONE &&
             !iwdg_reset_requested &&
             !(qspi_test_state >= QSPI_TEST_ERASE_START &&
               qspi_test_state <= QSPI_TEST_VERIFY))) {
          motor_test_request = requested_motor_test;
        } else if (requested_motor_test != BOARD_MOTOR_TEST_NONE) {
          help_requested = true;
        }
        uart_command_length = 0U;
      } else if (value != '\r') {
        if (value >= 32U && value <= 126U &&
            uart_command_length < sizeof(uart_command) - 1U) {
          uart_command[uart_command_length++] = (char)value;
        } else {
          uart_command_length = 0U;
          help_requested = true;
        }
      }
    }
  }

  QspiRunTest(now_ms);

  if (huart1.gState != HAL_UART_STATE_READY) {
    return telemetry_enabled;
  }

  if ((!initial_report_sent &&
       (fdcan_status != FDCAN_LOOPBACK_PENDING ||
        now_ms - self_test_started_ms >= SELF_TEST_REPORT_DELAY_MS)) ||
      report_requested) {
    const char *fdcan_text = "READY";
    const char *lcd_text = "DISABLED";
    const char *qspi_rw_text = "DISABLED";
    const char *iwdg_text = "DISABLED";
    char rtc_text[48];
    char qspi_text[96];
    const uint32_t configured_qspi_capacity =
        1UL << (hqspi1.Init.FlashSize + 1U);
    int length;

    rtc_read_ok =
        HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN) == HAL_OK &&
        HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN) == HAL_OK &&
        rtc_time.Hours <= 23U && rtc_time.Minutes <= 59U &&
        rtc_time.Seconds <= 59U && rtc_date.Month >= 1U &&
        rtc_date.Month <= 12U && rtc_date.Date >= 1U &&
        rtc_date.Date <= 31U;

    if (fdcan_status == FDCAN_LOOPBACK_PASSED) {
      fdcan_text = "PASS";
    } else if (fdcan_status == FDCAN_LOOPBACK_FAILED) {
      fdcan_text = "FAIL";
    }

    if (BspLcd_GetStatus() == BSP_LCD_DRAWING) {
      lcd_text = "DRAWING";
    } else if (BspLcd_GetStatus() == BSP_LCD_READY) {
      lcd_text = "READY";
    } else if (BspLcd_GetStatus() == BSP_LCD_FAILED) {
      lcd_text = "FAIL";
    }

    if (qspi_test_state >= QSPI_TEST_ERASE_START &&
        qspi_test_state <= QSPI_TEST_VERIFY) {
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
                     qspi_jedec_id[0], qspi_jedec_id[1],
                     qspi_jedec_id[2]);
    } else if (qspi_capacity_bytes != configured_qspi_capacity) {
      (void)snprintf(
          qspi_text, sizeof(qspi_text),
          "FAIL jedec=%02X%02X%02X detected=%luMiB configured=%luMiB",
          qspi_jedec_id[0], qspi_jedec_id[1], qspi_jedec_id[2],
          (unsigned long)(qspi_capacity_bytes / (1024UL * 1024UL)),
          (unsigned long)(configured_qspi_capacity / (1024UL * 1024UL)));
    } else {
      (void)snprintf(
          qspi_text, sizeof(qspi_text),
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
        "FDCAN_INTERNAL: %s\r\n"
        "FDCAN_EXTERNAL: DISABLED\r\n"
        "KEY: %s\r\n"
        "ENCODER: READY\r\n"
        "MOTOR: DISABLED\r\n"
        "IWDG_RESET_TEST: %s\r\n"
        "TELEMETRY: %s\r\n"
        "COMMANDS: ping, status, telemetry on, telemetry off, qspi test confirm, iwdg reset confirm, motor left forward confirm, motor left reverse confirm, motor right forward confirm, motor right reverse confirm, motor stop\r\n",
        rtc_text, qspi_text, qspi_rw_text, lcd_text, fdcan_text,
        button_test_passed ? "PASS" : "READY", iwdg_text,
        telemetry_enabled ? "ON" : "OFF");
    if (length > 0 && (size_t)length < sizeof(self_test_report) &&
        HAL_UART_Transmit_DMA(&huart1, (uint8_t *)self_test_report,
                              (uint16_t)length) == HAL_OK) {
      initial_report_sent = true;
      report_requested = false;
    }
    return telemetry_enabled;
  }

  if (pong_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)pong,
                            sizeof(pong) - 1U) == HAL_OK) {
    pong_requested = false;
    return telemetry_enabled;
  }

  if (telemetry_state_requested) {
    const uint8_t *message =
        telemetry_enabled ? telemetry_on : telemetry_off;
    const uint16_t message_length =
        telemetry_enabled ? sizeof(telemetry_on) - 1U
                          : sizeof(telemetry_off) - 1U;

    if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)message,
                              message_length) == HAL_OK) {
      telemetry_state_requested = false;
      return telemetry_enabled;
    }
  }

  if (qspi_test_report_requested) {
    const bool running =
        qspi_test_state >= QSPI_TEST_ERASE_START &&
        qspi_test_state <= QSPI_TEST_VERIFY;
    const char *result = running       ? "RUNNING"
                         : qspi_test_state == QSPI_TEST_PASSED
                             ? "PASS"
                             : "FAIL";
    const int length = snprintf(
        qspi_test_report, sizeof(qspi_test_report),
        "QSPI_RW_TEST: %s address=0x%08lX size=%u DMA\r\n",
        result, (unsigned long)QSPI_TEST_START,
        (unsigned int)QSPI_TEST_DATA_SIZE);

    if (length > 0 && (size_t)length < sizeof(qspi_test_report) &&
        HAL_UART_Transmit_DMA(&huart1, (uint8_t *)qspi_test_report,
                              (uint16_t)length) == HAL_OK) {
      qspi_test_report_requested = false;
      return telemetry_enabled;
    }
  }

  if (iwdg_reset_report_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)iwdg_armed,
                            sizeof(iwdg_armed) - 1U) == HAL_OK) {
    iwdg_reset_report_requested = false;
    return telemetry_enabled;
  }

  if (motor_test_report_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)motor_test_report,
                            (uint16_t)strlen(motor_test_report)) == HAL_OK) {
    motor_test_report_requested = false;
    return telemetry_enabled;
  }

  if (help_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)help,
                            sizeof(help) - 1U) == HAL_OK) {
    help_requested = false;
  }
  return telemetry_enabled;
}

void BoardSelfTest_GetStatus(BoardSelfTestStatus *status)
{
  if (status == NULL) {
    return;
  }
  status->qspi_id_valid = qspi_read_ok &&
                          qspi_capacity_bytes ==
                              8UL * 1024UL * 1024UL;
  status->qspi_capacity_bytes = qspi_capacity_bytes;
  status->iwdg_reset_test_passed = iwdg_reset_test_passed;
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

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart == &huart1 && size <= UART_RX_DMA_BUFFER_SIZE) {
    uart_rx_dma_position = size;
    uart_rx_event_pending = true;
  }
}
