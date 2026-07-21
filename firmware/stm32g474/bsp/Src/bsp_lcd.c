#include "bsp_lcd.h"

#include <stddef.h>
#include <stdint.h>

#include "main.h"
#include "spi.h"
#include "tim.h"

#define LCD_WIDTH 320U
#define LCD_HEIGHT 240U
#define LCD_SPI_TIMEOUT_MS 100U
#define LCD_BACKLIGHT_COMPARE 35U

#define LCD_COMMAND_SLEEP_OUT 0x11U
#define LCD_COMMAND_INVERSION_ON 0x21U
#define LCD_COMMAND_DISPLAY_ON 0x29U
#define LCD_COMMAND_COLUMN_ADDRESS 0x2AU
#define LCD_COMMAND_ROW_ADDRESS 0x2BU
#define LCD_COMMAND_MEMORY_WRITE 0x2CU
#define LCD_COMMAND_MEMORY_ACCESS 0x36U
#define LCD_COMMAND_PIXEL_FORMAT 0x3AU

#define LCD_COLOR_BLACK 0x0000U
#define LCD_COLOR_BLUE 0x001FU
#define LCD_COLOR_GREEN 0x07E0U
#define LCD_COLOR_RED 0xF800U
#define LCD_COLOR_WHITE 0xFFFFU
#define LCD_COLOR_YELLOW 0xFFE0U

static uint8_t lcd_line_buffer[LCD_WIDTH * 2U];
static volatile bool lcd_dma_complete;
static volatile bool lcd_dma_failed;
static uint16_t lcd_next_row;
static BspLcdStatus lcd_status;

static bool LcdWriteCommand(uint8_t command, const uint8_t *data,
                            uint16_t data_size)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(&hspi2, &command, 1U, LCD_SPI_TIMEOUT_MS) !=
      HAL_OK) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    return false;
  }

  if (data_size > 0U) {
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    if (HAL_SPI_Transmit(&hspi2, (uint8_t *)data, data_size,
                         LCD_SPI_TIMEOUT_MS) != HAL_OK) {
      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
      return false;
    }
  }
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  return true;
}

static bool LcdBeginFrame(void)
{
  static const uint8_t columns[] = {0x00U, 0x00U, 0x01U, 0x3FU};
  static const uint8_t rows[] = {0x00U, 0x00U, 0x00U, 0xEFU};
  uint8_t command = LCD_COMMAND_MEMORY_WRITE;

  if (!LcdWriteCommand(LCD_COMMAND_COLUMN_ADDRESS, columns,
                       sizeof(columns)) ||
      !LcdWriteCommand(LCD_COMMAND_ROW_ADDRESS, rows, sizeof(rows))) {
    return false;
  }

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(&hspi2, &command, 1U, LCD_SPI_TIMEOUT_MS) !=
      HAL_OK) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    return false;
  }
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  return true;
}

static bool LcdStartRow(uint16_t row)
{
  for (uint16_t column = 0U; column < LCD_WIDTH; ++column) {
    uint16_t color;

    if (row < 4U || row >= LCD_HEIGHT - 4U || column < 4U ||
        column >= LCD_WIDTH - 4U) {
      color = LCD_COLOR_WHITE;
    } else if (row >= LCD_HEIGHT / 2U - 2U &&
               row < LCD_HEIGHT / 2U + 2U) {
      color = LCD_COLOR_BLACK;
    } else if (column >= LCD_WIDTH / 2U - 2U &&
               column < LCD_WIDTH / 2U + 2U) {
      color = LCD_COLOR_BLACK;
    } else if (row < LCD_HEIGHT / 2U) {
      color = column < LCD_WIDTH / 2U ? LCD_COLOR_RED
                                      : LCD_COLOR_GREEN;
    } else {
      color = column < LCD_WIDTH / 2U ? LCD_COLOR_BLUE
                                      : LCD_COLOR_YELLOW;
    }

    lcd_line_buffer[column * 2U] = (uint8_t)(color >> 8U);
    lcd_line_buffer[column * 2U + 1U] = (uint8_t)color;
  }

  lcd_dma_complete = false;
  return HAL_SPI_Transmit_DMA(&hspi2, lcd_line_buffer,
                              sizeof(lcd_line_buffer)) == HAL_OK;
}

bool BspLcd_Init(void)
{
  static const uint8_t memory_access[] = {0x70U};
  static const uint8_t pixel_format[] = {0x05U};
  static const uint8_t porch[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
  static const uint8_t gate_control[] = {0x35U};
  static const uint8_t vcom[] = {0x32U};
  static const uint8_t command_enable[] = {0x01U};
  static const uint8_t vrh[] = {0x15U};
  static const uint8_t vdv[] = {0x20U};
  static const uint8_t frame_rate[] = {0x0FU};
  static const uint8_t power_control[] = {0xA4U, 0xA1U};
  static const uint8_t positive_gamma[] = {
      0xD0U, 0x08U, 0x0EU, 0x09U, 0x09U, 0x05U, 0x31U,
      0x33U, 0x48U, 0x17U, 0x14U, 0x15U, 0x31U, 0x34U};
  static const uint8_t negative_gamma[] = {
      0xD0U, 0x08U, 0x0EU, 0x09U, 0x09U, 0x15U, 0x31U,
      0x33U, 0x48U, 0x17U, 0x14U, 0x15U, 0x31U, 0x34U};

  lcd_status = BSP_LCD_FAILED;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) {
    return false;
  }

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(20U);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(120U);

  if (!LcdWriteCommand(LCD_COMMAND_SLEEP_OUT, NULL, 0U)) {
    return false;
  }
  HAL_Delay(120U);

  if (!LcdWriteCommand(LCD_COMMAND_MEMORY_ACCESS, memory_access,
                       sizeof(memory_access)) ||
      !LcdWriteCommand(LCD_COMMAND_PIXEL_FORMAT, pixel_format,
                       sizeof(pixel_format)) ||
      !LcdWriteCommand(0xB2U, porch, sizeof(porch)) ||
      !LcdWriteCommand(0xB7U, gate_control, sizeof(gate_control)) ||
      !LcdWriteCommand(0xBBU, vcom, sizeof(vcom)) ||
      !LcdWriteCommand(0xC2U, command_enable, sizeof(command_enable)) ||
      !LcdWriteCommand(0xC3U, vrh, sizeof(vrh)) ||
      !LcdWriteCommand(0xC4U, vdv, sizeof(vdv)) ||
      !LcdWriteCommand(0xC6U, frame_rate, sizeof(frame_rate)) ||
      !LcdWriteCommand(0xD0U, power_control, sizeof(power_control)) ||
      !LcdWriteCommand(0xE0U, positive_gamma, sizeof(positive_gamma)) ||
      !LcdWriteCommand(0xE1U, negative_gamma, sizeof(negative_gamma)) ||
      !LcdWriteCommand(LCD_COMMAND_INVERSION_ON, NULL, 0U) ||
      !LcdWriteCommand(LCD_COMMAND_DISPLAY_ON, NULL, 0U) ||
      !LcdBeginFrame()) {
    return false;
  }

  lcd_next_row = 1U;
  lcd_dma_failed = false;
  lcd_status = BSP_LCD_DRAWING;
  if (!LcdStartRow(0U)) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    lcd_status = BSP_LCD_FAILED;
    return false;
  }
  return true;
}

void BspLcd_Run(void)
{
  if (lcd_status != BSP_LCD_DRAWING) {
    return;
  }
  if (lcd_dma_failed) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    lcd_status = BSP_LCD_FAILED;
    return;
  }
  if (!lcd_dma_complete) {
    return;
  }
  if (lcd_next_row >= LCD_HEIGHT) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1,
                          LCD_BACKLIGHT_COMPARE);
    lcd_status = BSP_LCD_READY;
    return;
  }
  if (!LcdStartRow(lcd_next_row++)) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    lcd_status = BSP_LCD_FAILED;
  }
}

BspLcdStatus BspLcd_GetStatus(void)
{
  return lcd_status;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi2) {
    lcd_dma_complete = true;
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi2) {
    lcd_dma_failed = true;
  }
}
