#include "board_self_test.h"

#include <stdio.h>
#include <string.h>

#include "bsp_lcd.h"
#include "fdcan_driver.h"
#include "quadspi.h"
#include "rtc.h"
#include "usart.h"

#define UART_RX_DMA_BUFFER_SIZE 128U
#define UART_COMMAND_BUFFER_SIZE 32U
#define SELF_TEST_REPORT_DELAY_MS 100U
#define QSPI_JEDEC_ID_COMMAND 0x9FU
#define QSPI_WRITE_ENABLE_COMMAND 0x06U
#define QSPI_READ_STATUS_COMMAND 0x05U
#define QSPI_SECTOR_ERASE_COMMAND 0x20U
#define QSPI_PAGE_PROGRAM_COMMAND 0x02U
#define QSPI_READ_DATA_COMMAND 0x03U
#define QSPI_COMMAND_TIMEOUT_MS 100U
#define QSPI_ERASE_TIMEOUT_MS 5000U
#define QSPI_PROGRAM_TIMEOUT_MS 500U
#define QSPI_STATUS_BUSY 0x01U
#define QSPI_STATUS_WRITE_ENABLE_LATCH 0x02U
#define QSPI_TEST_SECTOR_ADDRESS 0x007FF000UL
#define QSPI_TEST_DATA_SIZE 32U

typedef enum
{
  QSPI_TEST_IDLE = 0,
  QSPI_TEST_ERASE_WRITE_ENABLE,
  QSPI_TEST_ERASE_START,
  QSPI_TEST_ERASE_WAIT,
  QSPI_TEST_ERASE_VERIFY,
  QSPI_TEST_PROGRAM_WRITE_ENABLE,
  QSPI_TEST_PROGRAM_START,
  QSPI_TEST_PROGRAM_WAIT,
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
static bool qspi_test_report_requested;
static uint8_t qspi_test_read_buffer[QSPI_TEST_DATA_SIZE];
static const uint8_t qspi_test_pattern[QSPI_TEST_DATA_SIZE] = {
    0x43U, 0x48U, 0x41U, 0x53U, 0x53U, 0x49U, 0x53U, 0x2DU,
    0x51U, 0x53U, 0x50U, 0x49U, 0x2DU, 0x54U, 0x45U, 0x53U,
    0x54U, 0x2DU, 0x30U, 0x31U, 0xA5U, 0x5AU, 0x3CU, 0xC3U,
    0x00U, 0xFFU, 0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU};
static bool initial_report_sent;
static bool report_requested;
static bool pong_requested;
static bool help_requested;
static bool telemetry_enabled;
static bool telemetry_state_requested;
static uint32_t self_test_started_ms;
static char self_test_report[576];
static char qspi_test_report[80];

static bool QspiReadStatus(uint8_t *status)
{
  QSPI_CommandTypeDef command = {0};

  command.Instruction = QSPI_READ_STATUS_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0U;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) ==
             HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, status, QSPI_COMMAND_TIMEOUT_MS) ==
             HAL_OK;
}

static bool QspiWriteEnable(void)
{
  QSPI_CommandTypeDef command = {0};
  uint8_t status;

  command.Instruction = QSPI_WRITE_ENABLE_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) ==
             HAL_OK &&
         QspiReadStatus(&status) &&
         (status & QSPI_STATUS_WRITE_ENABLE_LATCH) != 0U;
}

static bool QspiEraseTestSector(void)
{
  QSPI_CommandTypeDef command = {0};

  command.Instruction = QSPI_SECTOR_ERASE_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = QSPI_TEST_SECTOR_ADDRESS;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) ==
         HAL_OK;
}

static bool QspiReadTestData(void)
{
  QSPI_CommandTypeDef command = {0};

  command.Instruction = QSPI_READ_DATA_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = QSPI_TEST_SECTOR_ADDRESS;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0U;
  command.NbData = sizeof(qspi_test_read_buffer);
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) ==
             HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, qspi_test_read_buffer,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool QspiProgramTestData(void)
{
  QSPI_CommandTypeDef command = {0};

  command.Instruction = QSPI_PAGE_PROGRAM_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = QSPI_TEST_SECTOR_ADDRESS;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0U;
  command.NbData = sizeof(qspi_test_pattern);
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) ==
             HAL_OK &&
         HAL_QSPI_Transmit(&hqspi1, (uint8_t *)qspi_test_pattern,
                           QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static void QspiRunTest(uint32_t now_ms)
{
  uint8_t status;

  switch (qspi_test_state) {
    case QSPI_TEST_ERASE_WRITE_ENABLE:
      qspi_test_state = QspiWriteEnable() ? QSPI_TEST_ERASE_START
                                          : QSPI_TEST_FAILED;
      break;
    case QSPI_TEST_ERASE_START:
      if (QspiEraseTestSector()) {
        qspi_test_deadline_ms = now_ms + QSPI_ERASE_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_ERASE_WAIT;
      } else {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_ERASE_WAIT:
      if (!QspiReadStatus(&status)) {
        qspi_test_state = QSPI_TEST_FAILED;
      } else if ((status & QSPI_STATUS_BUSY) == 0U) {
        qspi_test_state = QSPI_TEST_ERASE_VERIFY;
      } else if ((int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_ERASE_VERIFY:
      if (!QspiReadTestData()) {
        qspi_test_state = QSPI_TEST_FAILED;
        break;
      }
      for (size_t index = 0U; index < sizeof(qspi_test_read_buffer);
           ++index) {
        if (qspi_test_read_buffer[index] != 0xFFU) {
          qspi_test_state = QSPI_TEST_FAILED;
          break;
        }
      }
      if (qspi_test_state == QSPI_TEST_ERASE_VERIFY) {
        qspi_test_state = QSPI_TEST_PROGRAM_WRITE_ENABLE;
      }
      break;
    case QSPI_TEST_PROGRAM_WRITE_ENABLE:
      qspi_test_state = QspiWriteEnable() ? QSPI_TEST_PROGRAM_START
                                          : QSPI_TEST_FAILED;
      break;
    case QSPI_TEST_PROGRAM_START:
      if (QspiProgramTestData()) {
        qspi_test_deadline_ms = now_ms + QSPI_PROGRAM_TIMEOUT_MS;
        qspi_test_state = QSPI_TEST_PROGRAM_WAIT;
      } else {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_PROGRAM_WAIT:
      if (!QspiReadStatus(&status)) {
        qspi_test_state = QSPI_TEST_FAILED;
      } else if ((status & QSPI_STATUS_BUSY) == 0U) {
        qspi_test_state = QSPI_TEST_VERIFY;
      } else if ((int32_t)(now_ms - qspi_test_deadline_ms) >= 0) {
        qspi_test_state = QSPI_TEST_FAILED;
      }
      break;
    case QSPI_TEST_VERIFY:
      qspi_test_state =
          QspiReadTestData() &&
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

  self_test_started_ms = HAL_GetTick();
  telemetry_enabled = false;
  qspi_test_state = QSPI_TEST_IDLE;

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
      "COMMANDS: ping, status, telemetry on, telemetry off, qspi test confirm\r\n";
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
          if (qspi_test_state == QSPI_TEST_IDLE ||
              qspi_test_state == QSPI_TEST_PASSED ||
              qspi_test_state == QSPI_TEST_FAILED) {
            qspi_test_state =
                qspi_read_ok &&
                        qspi_capacity_bytes == 8UL * 1024UL * 1024UL
                    ? QSPI_TEST_ERASE_WRITE_ENABLE
                    : QSPI_TEST_FAILED;
          }
          qspi_test_report_requested = true;
        } else if (uart_command_length != 0U) {
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

    if (qspi_test_state >= QSPI_TEST_ERASE_WRITE_ENABLE &&
        qspi_test_state <= QSPI_TEST_VERIFY) {
      qspi_rw_text = "RUNNING";
    } else if (qspi_test_state == QSPI_TEST_PASSED) {
      qspi_rw_text = "PASS";
    } else if (qspi_test_state == QSPI_TEST_FAILED) {
      qspi_rw_text = "FAIL";
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
        "ENCODER: READY\r\n"
        "MOTOR: DISABLED\r\n"
        "IWDG_RESET_TEST: DISABLED\r\n"
        "TELEMETRY: %s\r\n"
        "COMMANDS: ping, status, telemetry on, telemetry off, qspi test confirm\r\n",
        rtc_text, qspi_text, qspi_rw_text, lcd_text, fdcan_text,
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
        qspi_test_state >= QSPI_TEST_ERASE_WRITE_ENABLE &&
        qspi_test_state <= QSPI_TEST_VERIFY;
    const char *result = running       ? "RUNNING"
                         : qspi_test_state == QSPI_TEST_PASSED
                             ? "PASS"
                             : "FAIL";
    const int length = snprintf(
        qspi_test_report, sizeof(qspi_test_report),
        "QSPI_RW_TEST: %s address=0x%08lX\r\n", result,
        (unsigned long)QSPI_TEST_SECTOR_ADDRESS);

    if (length > 0 && (size_t)length < sizeof(qspi_test_report) &&
        HAL_UART_Transmit_DMA(&huart1, (uint8_t *)qspi_test_report,
                              (uint16_t)length) == HAL_OK) {
      qspi_test_report_requested = false;
      return telemetry_enabled;
    }
  }

  if (help_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)help,
                            sizeof(help) - 1U) == HAL_OK) {
    help_requested = false;
  }
  return telemetry_enabled;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart == &huart1 && size <= UART_RX_DMA_BUFFER_SIZE) {
    uart_rx_dma_position = size;
    uart_rx_event_pending = true;
  }
}
