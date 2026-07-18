#include "board_self_test.h"

#include <stdio.h>
#include <string.h>

#include "fdcan_driver.h"
#include "rtc.h"
#include "usart.h"

#define UART_RX_DMA_BUFFER_SIZE 128U
#define UART_COMMAND_BUFFER_SIZE 32U
#define SELF_TEST_REPORT_DELAY_MS 100U

static uint8_t uart_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];
static volatile uint16_t uart_rx_dma_position;
static volatile bool uart_rx_event_pending;
static uint16_t uart_rx_read_position;
static char uart_command[UART_COMMAND_BUFFER_SIZE];
static uint8_t uart_command_length;

static RTC_TimeTypeDef rtc_time;
static RTC_DateTypeDef rtc_date;
static bool rtc_read_ok;
static bool initial_report_sent;
static bool report_requested;
static bool pong_requested;
static bool help_requested;
static uint32_t self_test_started_ms;
static char self_test_report[384];

bool BoardSelfTest_Init(void)
{
  self_test_started_ms = HAL_GetTick();

  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer,
                                   sizeof(uart_rx_dma_buffer)) != HAL_OK) {
    return false;
  }
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  return true;
}

void BoardSelfTest_Run(uint32_t now_ms)
{
  static const uint8_t pong[] = "PONG\r\n";
  static const uint8_t help[] = "COMMANDS: ping, status\r\n";
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

  if (huart1.gState != HAL_UART_STATE_READY) {
    return;
  }

  if ((!initial_report_sent &&
       (fdcan_status != FDCAN_LOOPBACK_PENDING ||
        now_ms - self_test_started_ms >= SELF_TEST_REPORT_DELAY_MS)) ||
      report_requested) {
    const char *fdcan_text = "READY";
    char rtc_text[48];
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

    length = snprintf(
        self_test_report, sizeof(self_test_report),
        "SELF_TEST\r\n"
        "USART_TX_DMA: READY\r\n"
        "USART_RX_DMA: READY\r\n"
        "RTC_READ: %s\r\n"
        "RTC_BACKUP: READY\r\n"
        "QSPI_ID: READY\r\n"
        "QSPI_RW_TEST: DISABLED\r\n"
        "LCD: READY\r\n"
        "FDCAN_INTERNAL: %s\r\n"
        "FDCAN_EXTERNAL: DISABLED\r\n"
        "ENCODER: READY\r\n"
        "MOTOR: DISABLED\r\n"
        "IWDG_RESET_TEST: DISABLED\r\n"
        "COMMANDS: ping, status\r\n",
        rtc_text, fdcan_text);
    if (length > 0 && (size_t)length < sizeof(self_test_report) &&
        HAL_UART_Transmit_DMA(&huart1, (uint8_t *)self_test_report,
                              (uint16_t)length) == HAL_OK) {
      initial_report_sent = true;
      report_requested = false;
    }
    return;
  }

  if (pong_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)pong,
                            sizeof(pong) - 1U) == HAL_OK) {
    pong_requested = false;
    return;
  }

  if (help_requested &&
      HAL_UART_Transmit_DMA(&huart1, (uint8_t *)help,
                            sizeof(help) - 1U) == HAL_OK) {
    help_requested = false;
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart == &huart1 && size <= UART_RX_DMA_BUFFER_SIZE) {
    uart_rx_dma_position = size;
    uart_rx_event_pending = true;
  }
}
