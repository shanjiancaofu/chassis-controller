#ifdef FLASH_STM32_QSPI_HOST_TEST

#include <assert.h>

#include "drivers/flash/flash_stm32_qspi_private.h"

static QSPI_HandleTypeDef handle = {
    .Instance = QUADSPI,
    .State = HAL_QSPI_STATE_READY,
};
static FlashStm32QspiData data;
static const FlashStm32QspiConfig config = {.handle = &handle};
static struct device_state state = {.init_res = 0, .initialized = true};
const struct device device_flash0 = {
    .name = "flash0", .config = &config, .data = &data, .state = &state,
};

static uint32_t last_instruction;
static HAL_StatusTypeDef abort_result = HAL_OK;

HAL_StatusTypeDef HAL_QSPI_Command(QSPI_HandleTypeDef *qspi,
                                   QSPI_CommandTypeDef *command,
                                   uint32_t timeout)
{
  assert(qspi == &handle && timeout == 100U);
  last_instruction = command->Instruction;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_QSPI_Receive(QSPI_HandleTypeDef *qspi, uint8_t *buffer,
                                   uint32_t timeout)
{
  assert(qspi == &handle && buffer != NULL && timeout == 100U);
  buffer[0] = last_instruction == 0x05U ? 0x02U : 0xEFU;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_QSPI_Receive_DMA(QSPI_HandleTypeDef *qspi,
                                       uint8_t *buffer)
{
  assert(qspi == &handle && buffer != NULL);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_QSPI_Transmit_DMA(QSPI_HandleTypeDef *qspi,
                                        uint8_t *buffer)
{
  assert(qspi == &handle && buffer != NULL);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_QSPI_Abort(QSPI_HandleTypeDef *qspi)
{
  assert(qspi == &handle);
  return abort_result;
}

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *qspi);
void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *qspi);
void HAL_QSPI_ErrorCallback(QSPI_HandleTypeDef *qspi);

int main(void)
{
  const FlashDriverApi *api = &flash_stm32_qspi_api;
  uint8_t buffer[16] = {0};

  assert(FlashStm32Qspi_Init(&device_flash0) == 0);
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_IDLE);
  assert(api->read_dma(&device_flash0, 0U, buffer, sizeof(buffer)));
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_BUSY);
  HAL_QSPI_RxCpltCallback(&handle);
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_COMPLETE);

  assert(api->program_page_dma(&device_flash0, 0U, buffer, sizeof(buffer)));
  HAL_QSPI_TxCpltCallback(&handle);
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_COMPLETE);
  HAL_QSPI_ErrorCallback(&handle);
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_FAILED);

  api->abort(&device_flash0);
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_IDLE);
  abort_result = HAL_ERROR;
  api->abort(&device_flash0);
  assert(api->get_status(&device_flash0) == FLASH_TRANSFER_FAILED);
  assert(!api->read_dma(&device_flash0, 0x00800000U, buffer, 1U));
  return 0;
}

#endif
