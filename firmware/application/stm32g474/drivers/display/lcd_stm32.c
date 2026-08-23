#include "drivers/display/lcd_stm32_private.h"

#include <errno.h>
#include <stddef.h>

#include "main.h"
#include "spi.h"
#include "tim.h"

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

static DisplayStm32Data *Data(const struct device *device)
{
  return device != NULL ? (DisplayStm32Data *)device->data : NULL;
}

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

static bool InitDevice(const struct device *device)
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
  DisplayStm32Data *data = Data(device);

  if (data == NULL) {
    return false;
  }
  data->status = LCD_FAILED;
  data->dma_complete = false;
  data->dma_failed = false;
  data->dma_pending = false;
  __HAL_TIM_SET_COMPARE(&htim3,
                        TIM_CHANNEL_1, 0U);
  if (HAL_TIM_PWM_Start(&htim3,
                        TIM_CHANNEL_1) != HAL_OK) {
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
      !LcdWriteCommand(LCD_COMMAND_DISPLAY_ON, NULL, 0U)) {
    return false;
  }
  data->status = LCD_READY;
  return true;
}

static bool Begin(const struct device *device)
{
  static const uint8_t columns[] = {0x00U, 0x00U, 0x01U, 0x3FU};
  static const uint8_t rows[] = {0x00U, 0x00U, 0x00U, 0xEFU};
  uint8_t command = LCD_COMMAND_MEMORY_WRITE;
  DisplayStm32Data *data = Data(device);

  if (data == NULL || data->status != LCD_READY ||
      !LcdWriteCommand(LCD_COMMAND_COLUMN_ADDRESS, columns,
                       sizeof(columns)) ||
      !LcdWriteCommand(LCD_COMMAND_ROW_ADDRESS, rows, sizeof(rows))) {
    if (data != NULL) {
      data->status = LCD_FAILED;
    }
    return false;
  }
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(&hspi2, &command, 1U, LCD_SPI_TIMEOUT_MS) !=
      HAL_OK) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    data->status = LCD_FAILED;
    return false;
  }
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  data->dma_complete = false;
  data->dma_failed = false;
  data->dma_pending = false;
  data->status = LCD_TRANSMITTING;
  return true;
}

static bool Transmit(const struct device *device, const uint8_t *row_data,
                     uint16_t size)
{
  DisplayStm32Data *data = Data(device);
  if (row_data == NULL || size != LCD_WIDTH * 2U ||
      data == NULL || data->status != LCD_TRANSMITTING || data->dma_pending) {
    return false;
  }
  data->dma_complete = false;
  data->dma_pending = true;
  if (HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)row_data, size) !=
      HAL_OK) {
    data->dma_pending = false;
    data->status = LCD_FAILED;
    return false;
  }
  return true;
}

static bool Complete(const struct device *device)
{
  const DisplayStm32Data *data = Data(device);
  return data != NULL && data->status == LCD_TRANSMITTING &&
         data->dma_complete && !data->dma_pending;
}

static bool HasError(const struct device *device)
{
  const DisplayStm32Data *data = Data(device);
  return data == NULL || data->dma_failed || data->status == LCD_FAILED;
}

static void End(const struct device *device)
{
  DisplayStm32Data *data = Data(device);
  if (data == NULL) {
    return;
  }
  if (data->status != LCD_TRANSMITTING || data->dma_pending) {
    data->status = LCD_FAILED;
    return;
  }
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  __HAL_TIM_SET_COMPARE(&htim3,
                        TIM_CHANNEL_1,
                        LCD_BACKLIGHT_COMPARE);
  data->status = LCD_READY;
}

static LcdStatus Status(const struct device *device)
{
  const DisplayStm32Data *data = Data(device);
  return data != NULL ? data->status : LCD_DISABLED;
}

static void TxComplete(const struct device *device)
{
  DisplayStm32Data *data = Data(device);
  if (data != NULL) {
    data->dma_pending = false;
    data->dma_complete = true;
  }
}

static void Error(const struct device *device)
{
  DisplayStm32Data *data = Data(device);
  if (data != NULL) {
    data->dma_pending = false;
    data->dma_failed = true;
    data->status = LCD_FAILED;
  }
}

const DisplayDriverApi display_stm32_api = {
    .begin_frame=Begin,.transmit_row=Transmit,.row_complete=Complete,
    .has_error=HasError,.end_frame=End,.get_status=Status,
    .on_tx_complete=TxComplete,
    .on_error=Error,
};

int DisplayStm32_Init(const struct device *device)
{
  return InitDevice(device) ? 0 : -EIO;
}
