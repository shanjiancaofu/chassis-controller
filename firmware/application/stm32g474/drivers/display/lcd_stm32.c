#include "drivers/display/lcd.h"

#include <stddef.h>

#include "boards/chassis_g474/board_config.h"

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

static volatile bool lcd_dma_complete;
static volatile bool lcd_dma_failed;
static volatile bool lcd_dma_pending;
static LcdStatus lcd_status;

static bool LcdWriteCommand(uint8_t command, const uint8_t *data,
                            uint16_t data_size)
{
  HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BOARD_LCD_DC_GPIO_PORT, BOARD_LCD_DC_GPIO_PIN, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(&BOARD_LCD_SPI, &command, 1U, LCD_SPI_TIMEOUT_MS) !=
      HAL_OK) {
    HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_SET);
    return false;
  }
  if (data_size > 0U) {
    HAL_GPIO_WritePin(BOARD_LCD_DC_GPIO_PORT, BOARD_LCD_DC_GPIO_PIN, GPIO_PIN_SET);
    if (HAL_SPI_Transmit(&BOARD_LCD_SPI, (uint8_t *)data, data_size,
                         LCD_SPI_TIMEOUT_MS) != HAL_OK) {
      HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_SET);
      return false;
    }
  }
  HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_SET);
  return true;
}

bool Lcd_Init(void)
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

  lcd_status = LCD_FAILED;
  lcd_dma_complete = false;
  lcd_dma_failed = false;
  lcd_dma_pending = false;
  __HAL_TIM_SET_COMPARE(&BOARD_LCD_BACKLIGHT_TIMER,
                        BOARD_LCD_BACKLIGHT_CHANNEL, 0U);
  if (HAL_TIM_PWM_Start(&BOARD_LCD_BACKLIGHT_TIMER,
                        BOARD_LCD_BACKLIGHT_CHANNEL) != HAL_OK) {
    return false;
  }
  HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(BOARD_LCD_RESET_GPIO_PORT, BOARD_LCD_RESET_GPIO_PIN, GPIO_PIN_RESET);
  HAL_Delay(20U);
  HAL_GPIO_WritePin(BOARD_LCD_RESET_GPIO_PORT, BOARD_LCD_RESET_GPIO_PIN, GPIO_PIN_SET);
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
      !LcdWriteCommand(LCD_COMMAND_DISPLAY_ON, NULL, 0U)) {
    return false;
  }
  lcd_status = LCD_READY;
  return true;
}

bool Lcd_BeginFrame(void)
{
  static const uint8_t columns[] = {0x00U, 0x00U, 0x01U, 0x3FU};
  static const uint8_t rows[] = {0x00U, 0x00U, 0x00U, 0xEFU};
  uint8_t command = LCD_COMMAND_MEMORY_WRITE;

  if (lcd_status != LCD_READY ||
      !LcdWriteCommand(LCD_COMMAND_COLUMN_ADDRESS, columns,
                       sizeof(columns)) ||
      !LcdWriteCommand(LCD_COMMAND_ROW_ADDRESS, rows, sizeof(rows))) {
    lcd_status = LCD_FAILED;
    return false;
  }
  HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BOARD_LCD_DC_GPIO_PORT, BOARD_LCD_DC_GPIO_PIN, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(&BOARD_LCD_SPI, &command, 1U, LCD_SPI_TIMEOUT_MS) !=
      HAL_OK) {
    HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_SET);
    lcd_status = LCD_FAILED;
    return false;
  }
  HAL_GPIO_WritePin(BOARD_LCD_DC_GPIO_PORT, BOARD_LCD_DC_GPIO_PIN, GPIO_PIN_SET);
  lcd_dma_complete = false;
  lcd_dma_failed = false;
  lcd_dma_pending = false;
  lcd_status = LCD_TRANSMITTING;
  return true;
}

bool Lcd_TransmitRow(const uint8_t *row_data, uint16_t size)
{
  if (row_data == NULL || size != LCD_WIDTH * 2U ||
      lcd_status != LCD_TRANSMITTING || lcd_dma_pending) {
    return false;
  }
  lcd_dma_complete = false;
  lcd_dma_pending = true;
  if (HAL_SPI_Transmit_DMA(&BOARD_LCD_SPI, (uint8_t *)row_data, size) !=
      HAL_OK) {
    lcd_dma_pending = false;
    lcd_status = LCD_FAILED;
    return false;
  }
  return true;
}

bool Lcd_IsRowTransferComplete(void)
{
  return lcd_status == LCD_TRANSMITTING && lcd_dma_complete &&
         !lcd_dma_pending;
}

bool Lcd_HasTransferError(void)
{
  return lcd_dma_failed || lcd_status == LCD_FAILED;
}

void Lcd_EndFrame(void)
{
  if (lcd_status != LCD_TRANSMITTING || lcd_dma_pending) {
    lcd_status = LCD_FAILED;
    return;
  }
  HAL_GPIO_WritePin(BOARD_LCD_CS_GPIO_PORT, BOARD_LCD_CS_GPIO_PIN, GPIO_PIN_SET);
  __HAL_TIM_SET_COMPARE(&BOARD_LCD_BACKLIGHT_TIMER,
                        BOARD_LCD_BACKLIGHT_CHANNEL,
                        LCD_BACKLIGHT_COMPARE);
  lcd_status = LCD_READY;
}

LcdStatus Lcd_GetStatus(void)
{
  return lcd_status;
}

void Lcd_OnSpiTxComplete(void)
{
  lcd_dma_pending = false;
  lcd_dma_complete = true;
}

void Lcd_OnSpiError(void)
{
  lcd_dma_pending = false;
  lcd_dma_failed = true;
  lcd_status = LCD_FAILED;
}

static bool Begin(const struct device *device) { (void)device; return Lcd_BeginFrame(); }
static bool Transmit(const struct device *device,const uint8_t *data,uint16_t size) { (void)device; return Lcd_TransmitRow(data,size); }
static bool Complete(const struct device *device) { (void)device; return Lcd_IsRowTransferComplete(); }
static bool HasError(const struct device *device) { (void)device; return Lcd_HasTransferError(); }
static void End(const struct device *device) { (void)device; Lcd_EndFrame(); }
static LcdStatus Status(const struct device *device) { (void)device; return Lcd_GetStatus(); }

const DisplayDriverApi display_stm32_api = {
    .begin_frame=Begin,.transmit_row=Transmit,.row_complete=Complete,
    .has_error=HasError,.end_frame=End,.get_status=Status,
};

int DisplayStm32_Init(const struct device *device)
{
  (void)device;
  return Lcd_Init() ? 0 : -1;
}
