#include "bsp_lcd.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd_cover_image.h"
#include "main.h"
#include "spi.h"
#include "tim.h"

#define LCD_WIDTH 320U
#define LCD_HEIGHT 240U
#define LCD_SPI_TIMEOUT_MS 100U
#define LCD_BACKLIGHT_COMPARE 35U
#define LCD_TEXT_LINE_COUNT 7U
#define LCD_TEXT_LINE_LENGTH 28U

#define LCD_COMMAND_SLEEP_OUT 0x11U
#define LCD_COMMAND_INVERSION_ON 0x21U
#define LCD_COMMAND_DISPLAY_ON 0x29U
#define LCD_COMMAND_COLUMN_ADDRESS 0x2AU
#define LCD_COMMAND_ROW_ADDRESS 0x2BU
#define LCD_COMMAND_MEMORY_WRITE 0x2CU
#define LCD_COMMAND_MEMORY_ACCESS 0x36U
#define LCD_COMMAND_PIXEL_FORMAT 0x3AU

#define LCD_COLOR_BACKGROUND 0x18E3U
#define LCD_COLOR_COVER_MARGIN 0xFFFFU
#define LCD_COLOR_HEADER 0xFD20U
#define LCD_COLOR_TEXT 0xFFFFU
#define LCD_COLOR_PASS 0x07E0U
#define LCD_COLOR_FAIL 0xF800U
#define LCD_COLOR_READY 0xFFE0U
#define LCD_COLOR_DISABLED 0x8410U

typedef struct
{
  char character;
  uint8_t columns[5];
} LcdGlyph;

static const LcdGlyph lcd_glyphs[] = {
    {'-', {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}},
    {'.', {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}},
    {':', {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}},
    {'0', {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}},
    {'1', {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}},
    {'2', {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}},
    {'3', {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}},
    {'4', {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}},
    {'5', {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}},
    {'6', {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}},
    {'7', {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}},
    {'8', {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}},
    {'9', {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}},
    {'A', {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}},
    {'B', {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}},
    {'C', {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}},
    {'D', {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}},
    {'E', {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}},
    {'F', {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}},
    {'G', {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}},
    {'H', {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}},
    {'I', {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}},
    {'J', {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}},
    {'K', {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}},
    {'L', {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}},
    {'M', {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}},
    {'N', {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}},
    {'O', {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}},
    {'P', {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}},
    {'Q', {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}},
    {'R', {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}},
    {'S', {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}},
    {'T', {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}},
    {'U', {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}},
    {'V', {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}},
    {'W', {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}},
    {'X', {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}},
    {'Y', {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}},
    {'Z', {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}},
};

static const uint16_t lcd_text_y[LCD_TEXT_LINE_COUNT] = {
    10U, 48U, 78U, 108U, 138U, 168U, 220U};
static uint8_t lcd_line_buffer[LCD_WIDTH * 2U];
static volatile bool lcd_dma_complete;
static volatile bool lcd_dma_failed;
static uint16_t lcd_next_row;
static BspLcdStatus lcd_status;
static BspLcdPage lcd_page;
static BspLcdPage lcd_requested_page;
static bool lcd_redraw_requested;
static BspLcdStatusData lcd_status_data;
static char lcd_text_lines[LCD_TEXT_LINE_COUNT][LCD_TEXT_LINE_LENGTH];
static uint16_t lcd_text_colors[LCD_TEXT_LINE_COUNT];

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

static const uint8_t *LcdFindGlyph(char character)
{
  if (character == ' ') {
    return NULL;
  }
  for (size_t index = 0U;
       index < sizeof(lcd_glyphs) / sizeof(lcd_glyphs[0]); ++index) {
    if (lcd_glyphs[index].character == character) {
      return lcd_glyphs[index].columns;
    }
  }
  return NULL;
}

static uint16_t LcdValueColor(BspLcdValueState state)
{
  switch (state) {
    case BSP_LCD_VALUE_PASS:
      return LCD_COLOR_PASS;
    case BSP_LCD_VALUE_FAIL:
      return LCD_COLOR_FAIL;
    case BSP_LCD_VALUE_DISABLED:
      return LCD_COLOR_DISABLED;
    default:
      return LCD_COLOR_READY;
  }
}

static const char *LcdValueText(BspLcdValueState state)
{
  switch (state) {
    case BSP_LCD_VALUE_PASS:
      return "PASS";
    case BSP_LCD_VALUE_FAIL:
      return "FAIL";
    case BSP_LCD_VALUE_DISABLED:
      return "DISABLED";
    default:
      return "READY";
  }
}

static void LcdPrepareStatusPage(void)
{
  (void)snprintf(lcd_text_lines[0], LCD_TEXT_LINE_LENGTH,
                 "CHASSIS STATUS");
  if (lcd_status_data.rtc_state == BSP_LCD_VALUE_PASS) {
    (void)snprintf(lcd_text_lines[1], LCD_TEXT_LINE_LENGTH,
                   "RTC   %02u:%02u:%02u",
                   (unsigned int)lcd_status_data.rtc_hours,
                   (unsigned int)lcd_status_data.rtc_minutes,
                   (unsigned int)lcd_status_data.rtc_seconds);
  } else {
    (void)snprintf(lcd_text_lines[1], LCD_TEXT_LINE_LENGTH, "RTC   %s",
                   LcdValueText(lcd_status_data.rtc_state));
  }
  (void)snprintf(lcd_text_lines[2], LCD_TEXT_LINE_LENGTH,
                 "QSPI  %s %uMIB",
                 LcdValueText(lcd_status_data.qspi_state),
                 (unsigned int)lcd_status_data.qspi_capacity_mib);
  (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH, "CAN   %s",
                 LcdValueText(lcd_status_data.can_state));
  if (lcd_status_data.adc_valid) {
    (void)snprintf(lcd_text_lines[4], LCD_TEXT_LINE_LENGTH,
                   "ADC   %luMV", (unsigned long)lcd_status_data.adc_mv);
  } else {
    (void)snprintf(lcd_text_lines[4], LCD_TEXT_LINE_LENGTH,
                   "ADC   FAIL");
  }
  (void)snprintf(lcd_text_lines[5], LCD_TEXT_LINE_LENGTH,
                 "FAULT 0X%08lX",
                 (unsigned long)lcd_status_data.fault_flags);
  (void)snprintf(lcd_text_lines[6], LCD_TEXT_LINE_LENGTH,
                 "KEY: COVER");

  lcd_text_colors[0] = LCD_COLOR_HEADER;
  lcd_text_colors[1] = LcdValueColor(lcd_status_data.rtc_state);
  lcd_text_colors[2] = LcdValueColor(lcd_status_data.qspi_state);
  lcd_text_colors[3] = LcdValueColor(lcd_status_data.can_state);
  lcd_text_colors[4] = lcd_status_data.adc_valid ? LCD_COLOR_PASS
                                                 : LCD_COLOR_FAIL;
  lcd_text_colors[5] = lcd_status_data.fault_flags == 0U
                           ? LCD_COLOR_PASS
                           : LCD_COLOR_FAIL;
  lcd_text_colors[6] = LCD_COLOR_TEXT;
}

static void LcdDrawTextOnRow(uint16_t row, uint16_t y, const char *text,
                             uint16_t color, uint8_t scale)
{
  uint16_t x = 12U;
  uint8_t font_row;

  if (row < y || row >= y + 7U * scale) {
    return;
  }
  font_row = (uint8_t)((row - y) / scale);

  while (*text != '\0' && x + 5U * scale <= LCD_WIDTH) {
    const uint8_t *glyph = LcdFindGlyph(*text++);

    if (glyph != NULL) {
      for (uint8_t column = 0U; column < 5U; ++column) {
        if ((glyph[column] & (1U << font_row)) != 0U) {
          for (uint8_t pixel = 0U; pixel < scale; ++pixel) {
            const uint16_t draw_x = x + column * scale + pixel;

            lcd_line_buffer[draw_x * 2U] = (uint8_t)(color >> 8U);
            lcd_line_buffer[draw_x * 2U + 1U] = (uint8_t)color;
          }
        }
      }
    }
    x += 6U * scale;
  }
}

static bool LcdStartRow(uint16_t row)
{
  const uint16_t cover_start_x =
      (LCD_WIDTH - LCD_COVER_IMAGE_WIDTH) / 2U;

  for (uint16_t column = 0U; column < LCD_WIDTH; ++column) {
    uint16_t color = lcd_page == BSP_LCD_PAGE_COVER
                         ? LCD_COLOR_COVER_MARGIN
                         : LCD_COLOR_BACKGROUND;

    if (lcd_page == BSP_LCD_PAGE_COVER &&
        column >= cover_start_x &&
        column < cover_start_x + LCD_COVER_IMAGE_WIDTH) {
      const uint32_t image_index =
          (row * LCD_COVER_IMAGE_WIDTH + column - cover_start_x) * 2U;

      lcd_line_buffer[column * 2U] =
          LCD_COVER_IMAGE_DATA[image_index];
      lcd_line_buffer[column * 2U + 1U] =
          LCD_COVER_IMAGE_DATA[image_index + 1U];
      continue;
    }

    lcd_line_buffer[column * 2U] = (uint8_t)(color >> 8U);
    lcd_line_buffer[column * 2U + 1U] = (uint8_t)color;
  }

  if (lcd_page == BSP_LCD_PAGE_STATUS) {
    for (uint8_t line = 0U; line < LCD_TEXT_LINE_COUNT; ++line) {
      LcdDrawTextOnRow(row, lcd_text_y[line], lcd_text_lines[line],
                       lcd_text_colors[line], line == 6U ? 1U : 2U);
    }
  }

  lcd_dma_complete = false;
  return HAL_SPI_Transmit_DMA(&hspi2, lcd_line_buffer,
                              sizeof(lcd_line_buffer)) == HAL_OK;
}

static bool LcdStartDrawing(void)
{
  lcd_page = lcd_requested_page;
  lcd_redraw_requested = false;
  if (lcd_page == BSP_LCD_PAGE_STATUS) {
    LcdPrepareStatusPage();
  }
  if (!LcdBeginFrame()) {
    return false;
  }

  lcd_next_row = 1U;
  lcd_dma_failed = false;
  lcd_status = BSP_LCD_DRAWING;
  return LcdStartRow(0U);
}

bool BspLcd_Init(void)
{
  static const uint8_t memory_access[] = {0xA0U};
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
  lcd_requested_page = BSP_LCD_PAGE_COVER;
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
      !LcdStartDrawing()) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    lcd_status = BSP_LCD_FAILED;
    return false;
  }
  return true;
}

void BspLcd_Run(void)
{
  if (lcd_status == BSP_LCD_READY && lcd_redraw_requested) {
    if (!LcdStartDrawing()) {
      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
      lcd_status = BSP_LCD_FAILED;
    }
    return;
  }
  if (lcd_status != BSP_LCD_DRAWING) {
    return;
  }
  if (lcd_dma_failed) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
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
    lcd_status = BSP_LCD_FAILED;
  }
}

void BspLcd_SetPage(BspLcdPage page)
{
  if (page != BSP_LCD_PAGE_COVER && page != BSP_LCD_PAGE_STATUS) {
    return;
  }
  if (lcd_requested_page != page) {
    lcd_requested_page = page;
    lcd_redraw_requested = true;
  }
}

void BspLcd_SetStatusData(const BspLcdStatusData *data)
{
  if (data == NULL) {
    return;
  }
  lcd_status_data = *data;
  if (lcd_requested_page == BSP_LCD_PAGE_STATUS) {
    lcd_redraw_requested = true;
  }
}

BspLcdStatus BspLcd_GetStatus(void)
{
  return lcd_status;
}

BspLcdPage BspLcd_GetPage(void)
{
  return lcd_requested_page;
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
