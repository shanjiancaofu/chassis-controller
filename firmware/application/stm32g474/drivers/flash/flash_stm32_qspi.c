#include "drivers/flash/flash_stm32_qspi_private.h"
#include "drivers/flash.h"
#include "device.h"

#include <stddef.h>

#include "quadspi.h"
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

static volatile FlashStm32QspiTransferStatus qspi_transfer_status;

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

bool FlashStm32QspiReadJedecId(uint8_t jedec_id[3])
{
  QSPI_CommandTypeDef command = {0};

  if (jedec_id == NULL || hqspi1.State != HAL_QSPI_STATE_READY) {
    return false;
  }

  command.Instruction = QSPI_JEDEC_ID_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0U;
  command.NbData = 3U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) ==
             HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, jedec_id,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

bool FlashStm32QspiRead(uint32_t address, uint8_t *data, uint32_t size)
{
  QSPI_CommandTypeDef command = {0};

  if (data == NULL || size == 0U || size > QSPI_DMA_MAX_TRANSFER_SIZE ||
      address >= QSPI_FLASH_CAPACITY_BYTES ||
      size > QSPI_FLASH_CAPACITY_BYTES - address ||
      hqspi1.State != HAL_QSPI_STATE_READY) {
    return false;
  }

  command.Instruction = QSPI_FAST_READ_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 8U;
  command.NbData = size;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, data,
                          QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

bool FlashStm32QspiReadDma(uint32_t address, uint8_t *data,
                          uint32_t size)
{
  QSPI_CommandTypeDef command = {0};

  if (data == NULL || size == 0U || size > QSPI_DMA_MAX_TRANSFER_SIZE ||
      address >= QSPI_FLASH_CAPACITY_BYTES ||
      size > QSPI_FLASH_CAPACITY_BYTES - address ||
      hqspi1.State != HAL_QSPI_STATE_READY) {
    return false;
  }

  command.Instruction = QSPI_FAST_READ_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 8U;
  command.NbData = size;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_BUSY;
  if (HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) !=
          HAL_OK ||
      HAL_QSPI_Receive_DMA(&hqspi1, data) != HAL_OK) {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_FAILED;
    return false;
  }
  return true;
}

bool FlashStm32QspiEraseSector(uint32_t address)
{
  QSPI_CommandTypeDef command = {0};

  if (address % QSPI_FLASH_SECTOR_SIZE != 0U ||
      address > QSPI_FLASH_CAPACITY_BYTES - QSPI_FLASH_SECTOR_SIZE ||
      hqspi1.State != HAL_QSPI_STATE_READY || !QspiWriteEnable()) {
    return false;
  }

  command.Instruction = QSPI_SECTOR_ERASE_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
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

bool FlashStm32QspiProgramPageDma(uint32_t address,
                                 const uint8_t *data,
                                 uint32_t size)
{
  QSPI_CommandTypeDef command = {0};
  const uint32_t page_offset = address % QSPI_FLASH_PAGE_SIZE;

  if (data == NULL || size == 0U || size > QSPI_FLASH_PAGE_SIZE ||
      page_offset + size > QSPI_FLASH_PAGE_SIZE ||
      address >= QSPI_FLASH_CAPACITY_BYTES ||
      size > QSPI_FLASH_CAPACITY_BYTES - address ||
      hqspi1.State != HAL_QSPI_STATE_READY || !QspiWriteEnable()) {
    return false;
  }

  command.Instruction = QSPI_PAGE_PROGRAM_COMMAND;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0U;
  command.NbData = size;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_BUSY;
  if (HAL_QSPI_Command(&hqspi1, &command, QSPI_COMMAND_TIMEOUT_MS) !=
          HAL_OK ||
      HAL_QSPI_Transmit_DMA(&hqspi1, (uint8_t *)data) != HAL_OK) {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_FAILED;
    return false;
  }
  return true;
}

bool FlashStm32QspiIsBusy(bool *busy)
{
  uint8_t status;

  if (busy == NULL || hqspi1.State != HAL_QSPI_STATE_READY ||
      !QspiReadStatus(&status)) {
    return false;
  }
  *busy = (status & QSPI_STATUS_BUSY) != 0U;
  return true;
}

FlashStm32QspiTransferStatus FlashStm32QspiGetTransferStatus(void)
{
  return qspi_transfer_status;
}

void FlashStm32QspiAbort(void)
{
  if (HAL_QSPI_Abort(&hqspi1) != HAL_OK) {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_FAILED;
  } else {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_IDLE;
  }
}

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
  if (hqspi == &hqspi1) {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_COMPLETE;
  }
}

void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
  if (hqspi == &hqspi1) {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_COMPLETE;
  }
}

void HAL_QSPI_ErrorCallback(QSPI_HandleTypeDef *hqspi)
{
  if (hqspi == &hqspi1) {
    qspi_transfer_status = FLASH_STM32_QSPI_TRANSFER_FAILED;
  }
}

static bool ApiId(const struct device *d,uint8_t id[3]){(void)d;return FlashStm32QspiReadJedecId(id);}
static bool ApiRead(const struct device*d,uint32_t a,uint8_t*b,uint32_t s){(void)d;return FlashStm32QspiRead(a,b,s);}
static bool ApiReadDma(const struct device*d,uint32_t a,uint8_t*b,uint32_t s){(void)d;return FlashStm32QspiReadDma(a,b,s);}
static bool ApiErase(const struct device*d,uint32_t a){(void)d;return FlashStm32QspiEraseSector(a);}
static bool ApiProgram(const struct device*d,uint32_t a,const uint8_t*b,uint32_t s){(void)d;return FlashStm32QspiProgramPageDma(a,b,s);}
static bool ApiBusy(const struct device*d,bool*b){(void)d;return FlashStm32QspiIsBusy(b);}
static FlashTransferStatus ApiStatus(const struct device*d){(void)d; switch(FlashStm32QspiGetTransferStatus()){case FLASH_STM32_QSPI_TRANSFER_BUSY:return FLASH_TRANSFER_BUSY;case FLASH_STM32_QSPI_TRANSFER_COMPLETE:return FLASH_TRANSFER_COMPLETE;case FLASH_STM32_QSPI_TRANSFER_FAILED:return FLASH_TRANSFER_FAILED;default:return FLASH_TRANSFER_IDLE;}}
static void ApiAbort(const struct device*d){(void)d;FlashStm32QspiAbort();}
const FlashDriverApi flash_stm32_qspi_api={.read_jedec_id=ApiId,.read=ApiRead,.read_dma=ApiReadDma,.erase_sector=ApiErase,.program_page_dma=ApiProgram,.is_busy=ApiBusy,.get_status=ApiStatus,.abort=ApiAbort};
int FlashStm32Qspi_Init(const struct device *device){(void)device;qspi_transfer_status=FLASH_STM32_QSPI_TRANSFER_IDLE;return hqspi1.Instance==QUADSPI?0:-1;}
