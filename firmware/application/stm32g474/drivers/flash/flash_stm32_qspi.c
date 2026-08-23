#include "drivers/flash/flash_stm32_qspi_private.h"

#include <errno.h>
#include <stddef.h>

#include "devicetree.h"
#include "../../../../shared/qspi_flash_identity.h"

#define QSPI_WRITE_ENABLE_COMMAND 0x06U
#define QSPI_JEDEC_ID_COMMAND 0x9FU
#define QSPI_READ_STATUS_COMMAND 0x05U
#define QSPI_SECTOR_ERASE_COMMAND 0x20U
#define QSPI_FAST_READ_COMMAND 0x0BU
#define QSPI_PAGE_PROGRAM_COMMAND 0x02U
#define QSPI_COMMAND_TIMEOUT_MS 100U
#define QSPI_DMA_MAX_TRANSFER_SIZE 65535UL
#define QSPI_STATUS_BUSY 0x01U
#define QSPI_STATUS_WRITE_ENABLE_LATCH 0x02U

static const FlashStm32QspiConfig *Config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static FlashStm32QspiData *Data(const struct device *device)
{
  return device != NULL ? (FlashStm32QspiData *)device->data : NULL;
}

static const struct device *DeviceFromHandle(QSPI_HandleTypeDef *handle)
{
  const struct device *device = DEVICE_DT_GET(DT_NODELABEL(flash0));
  const FlashStm32QspiConfig *config = Config(device);
  return config != NULL && config->handle == handle ? device : NULL;
}

static bool ReadStatus(const struct device *device, uint8_t *status)
{
  const FlashStm32QspiConfig *config = Config(device);
  QSPI_CommandTypeDef command = {0};
  if (config == NULL || status == NULL) {
    return false;
  }
  command.Instruction = QSPI_READ_STATUS_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
  return HAL_QSPI_Command(config->handle, &command,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(config->handle, status,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool WriteEnable(const struct device *device)
{
  const FlashStm32QspiConfig *config = Config(device);
  QSPI_CommandTypeDef command = {0};
  uint8_t status;
  if (config == NULL) {
    return false;
  }
  command.Instruction = QSPI_WRITE_ENABLE_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
  return HAL_QSPI_Command(config->handle, &command,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         ReadStatus(device, &status) &&
         (status & QSPI_STATUS_WRITE_ENABLE_LATCH) != 0U;
}

static bool ReadJedecId(const struct device *device, uint8_t id[3])
{
  const FlashStm32QspiConfig *config = Config(device);
  QSPI_CommandTypeDef command = {0};
  if (config == NULL || id == NULL ||
      config->handle->State != HAL_QSPI_STATE_READY) {
    return false;
  }
  command.Instruction = QSPI_JEDEC_ID_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 3U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
  return HAL_QSPI_Command(config->handle, &command,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(config->handle, id,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool ValidRange(uint32_t address, uint32_t size)
{
  return size > 0U && size <= QSPI_DMA_MAX_TRANSFER_SIZE &&
         address < QSPI_FLASH_CAPACITY_BYTES &&
         size <= QSPI_FLASH_CAPACITY_BYTES - address;
}

static void PrepareReadCommand(QSPI_CommandTypeDef *command,
                               uint32_t address, uint32_t size)
{
  *command = (QSPI_CommandTypeDef){0};
  command->Instruction = QSPI_FAST_READ_COMMAND;
  command->InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command->Address = address;
  command->AddressMode = QSPI_ADDRESS_1_LINE;
  command->AddressSize = QSPI_ADDRESS_24_BITS;
  command->AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command->DataMode = QSPI_DATA_1_LINE;
  command->DummyCycles = 8U;
  command->NbData = size;
  command->DdrMode = QSPI_DDR_MODE_DISABLE;
  command->DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command->SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
}

static bool Read(const struct device *device, uint32_t address, uint8_t *buffer,
                 uint32_t size)
{
  const FlashStm32QspiConfig *config = Config(device);
  QSPI_CommandTypeDef command;
  if (config == NULL || buffer == NULL || !ValidRange(address, size) ||
      config->handle->State != HAL_QSPI_STATE_READY) {
    return false;
  }
  PrepareReadCommand(&command, address, size);
  return HAL_QSPI_Command(config->handle, &command,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(config->handle, buffer,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool ReadDma(const struct device *device, uint32_t address,
                    uint8_t *buffer, uint32_t size)
{
  const FlashStm32QspiConfig *config = Config(device);
  FlashStm32QspiData *data = Data(device);
  QSPI_CommandTypeDef command;
  if (config == NULL || data == NULL || buffer == NULL ||
      !ValidRange(address, size) ||
      config->handle->State != HAL_QSPI_STATE_READY) {
    return false;
  }
  PrepareReadCommand(&command, address, size);
  data->transfer_status = FLASH_TRANSFER_BUSY;
  if (HAL_QSPI_Command(config->handle, &command,
                       QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
      HAL_QSPI_Receive_DMA(config->handle, buffer) != HAL_OK) {
    data->transfer_status = FLASH_TRANSFER_FAILED;
    return false;
  }
  return true;
}

static bool EraseSector(const struct device *device, uint32_t address)
{
  const FlashStm32QspiConfig *config = Config(device);
  QSPI_CommandTypeDef command = {0};
  if (config == NULL || address % QSPI_FLASH_SECTOR_SIZE != 0U ||
      address > QSPI_FLASH_CAPACITY_BYTES - QSPI_FLASH_SECTOR_SIZE ||
      config->handle->State != HAL_QSPI_STATE_READY || !WriteEnable(device)) {
    return false;
  }
  command.Instruction = QSPI_SECTOR_ERASE_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
  return HAL_QSPI_Command(config->handle, &command,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool ProgramPageDma(const struct device *device, uint32_t address,
                           const uint8_t *buffer, uint32_t size)
{
  const FlashStm32QspiConfig *config = Config(device);
  FlashStm32QspiData *data = Data(device);
  QSPI_CommandTypeDef command = {0};
  const uint32_t page_offset = address % QSPI_FLASH_PAGE_SIZE;
  if (config == NULL || data == NULL || buffer == NULL || size == 0U ||
      size > QSPI_FLASH_PAGE_SIZE ||
      page_offset + size > QSPI_FLASH_PAGE_SIZE ||
      address >= QSPI_FLASH_CAPACITY_BYTES ||
      size > QSPI_FLASH_CAPACITY_BYTES - address ||
      config->handle->State != HAL_QSPI_STATE_READY || !WriteEnable(device)) {
    return false;
  }
  command.Instruction = QSPI_PAGE_PROGRAM_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = size;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
  data->transfer_status = FLASH_TRANSFER_BUSY;
  if (HAL_QSPI_Command(config->handle, &command,
                       QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
      HAL_QSPI_Transmit_DMA(config->handle, (uint8_t *)buffer) != HAL_OK) {
    data->transfer_status = FLASH_TRANSFER_FAILED;
    return false;
  }
  return true;
}

static bool IsBusy(const struct device *device, bool *busy)
{
  const FlashStm32QspiConfig *config = Config(device);
  uint8_t status;
  if (config == NULL || busy == NULL ||
      config->handle->State != HAL_QSPI_STATE_READY ||
      !ReadStatus(device, &status)) {
    return false;
  }
  *busy = (status & QSPI_STATUS_BUSY) != 0U;
  return true;
}

static FlashTransferStatus GetStatus(const struct device *device)
{
  const FlashStm32QspiData *data = Data(device);
  return data != NULL ? data->transfer_status : FLASH_TRANSFER_FAILED;
}

static void Abort(const struct device *device)
{
  const FlashStm32QspiConfig *config = Config(device);
  FlashStm32QspiData *data = Data(device);
  if (config == NULL || data == NULL) {
    return;
  }
  data->transfer_status =
      HAL_QSPI_Abort(config->handle) == HAL_OK ? FLASH_TRANSFER_IDLE
                                               : FLASH_TRANSFER_FAILED;
}

const FlashDriverApi flash_stm32_qspi_api = {
    .read_jedec_id = ReadJedecId,
    .read = Read,
    .read_dma = ReadDma,
    .erase_sector = EraseSector,
    .program_page_dma = ProgramPageDma,
    .is_busy = IsBusy,
    .get_status = GetStatus,
    .abort = Abort,
};

int FlashStm32Qspi_Init(const struct device *device)
{
  const FlashStm32QspiConfig *config = Config(device);
  FlashStm32QspiData *data = Data(device);
  if (config == NULL || config->handle == NULL || data == NULL ||
      config->handle->Instance != QUADSPI) {
    return -EINVAL;
  }
  data->transfer_status = FLASH_TRANSFER_IDLE;
  return 0;
}

static void CompleteTransfer(QSPI_HandleTypeDef *handle,
                             FlashTransferStatus status)
{
  const struct device *device = DeviceFromHandle(handle);
  FlashStm32QspiData *data = Data(device);
  if (data != NULL) {
    data->transfer_status = status;
  }
}

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *handle)
{
  CompleteTransfer(handle, FLASH_TRANSFER_COMPLETE);
}

void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *handle)
{
  CompleteTransfer(handle, FLASH_TRANSFER_COMPLETE);
}

void HAL_QSPI_ErrorCallback(QSPI_HandleTypeDef *handle)
{
  CompleteTransfer(handle, FLASH_TRANSFER_FAILED);
}
