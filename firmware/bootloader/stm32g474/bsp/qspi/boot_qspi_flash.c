#include "bsp/qspi/boot_qspi_flash.h"

#include <stddef.h>

#include "iwdg.h"
#include "quadspi.h"

#define W25Q_WRITE_ENABLE 0x06U
#define W25Q_READ_STATUS_1 0x05U
#define W25Q_READ_JEDEC_ID 0x9FU
#define W25Q_FAST_READ 0x0BU
#define W25Q_PAGE_PROGRAM 0x02U
#define W25Q_SECTOR_ERASE 0x20U
#define W25Q_STATUS_BUSY 0x01U
#define W25Q_STATUS_WEL 0x02U
#define BOOT_QSPI_COMMAND_TIMEOUT_MS 100U
#define BOOT_QSPI_WRITE_TIMEOUT_MS 5000U

static bool ReadStatus(uint8_t *status);
static bool WriteEnable(void);
static bool WaitReady(uint32_t timeout_ms);

bool BootQspiFlash_ReadJedecId(uint8_t jedec_id[3])
{
  QSPI_CommandTypeDef command = {0};

  if (jedec_id == NULL) {
    return false;
  }

  command.Instruction = W25Q_READ_JEDEC_ID;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 3U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, jedec_id,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

bool BootQspiFlash_Read(uint32_t address, void *data, uint32_t size)
{
  QSPI_CommandTypeDef command = {0};

  if (data == NULL || size == 0U || address >= BOOT_QSPI_CAPACITY_BYTES ||
      size > BOOT_QSPI_CAPACITY_BYTES - address) {
    return false;
  }

  command.Instruction = W25Q_FAST_READ;
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
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, (uint8_t *)data,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

bool BootQspiFlash_EraseSector(uint32_t address)
{
  QSPI_CommandTypeDef command = {0};

  if ((address % BOOT_QSPI_SECTOR_SIZE) != 0U ||
      address > BOOT_QSPI_CAPACITY_BYTES - BOOT_QSPI_SECTOR_SIZE ||
      !WaitReady(BOOT_QSPI_WRITE_TIMEOUT_MS) || !WriteEnable()) {
    return false;
  }

  command.Instruction = W25Q_SECTOR_ERASE;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Address = address;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         WaitReady(BOOT_QSPI_WRITE_TIMEOUT_MS);
}

bool BootQspiFlash_Program(uint32_t address, const void *data,
                           uint32_t size)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t remaining = size;

  if (data == NULL || size == 0U || address >= BOOT_QSPI_CAPACITY_BYTES ||
      size > BOOT_QSPI_CAPACITY_BYTES - address) {
    return false;
  }

  while (remaining > 0U) {
    QSPI_CommandTypeDef command = {0};
    const uint32_t page_space =
        BOOT_QSPI_PAGE_SIZE - (address % BOOT_QSPI_PAGE_SIZE);
    const uint32_t chunk = remaining < page_space ? remaining : page_space;

    if (!WaitReady(BOOT_QSPI_WRITE_TIMEOUT_MS) || !WriteEnable()) {
      return false;
    }

    command.Instruction = W25Q_PAGE_PROGRAM;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Address = address;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = chunk;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(&hqspi1, &command,
                         BOOT_QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
        HAL_QSPI_Transmit(&hqspi1, (uint8_t *)bytes,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
        !WaitReady(BOOT_QSPI_WRITE_TIMEOUT_MS)) {
      return false;
    }

    address += chunk;
    bytes += chunk;
    remaining -= chunk;
  }

  return true;
}

static bool ReadStatus(uint8_t *status)
{
  QSPI_CommandTypeDef command = {0};

  command.Instruction = W25Q_READ_STATUS_1;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(&hqspi1, status,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool WriteEnable(void)
{
  QSPI_CommandTypeDef command = {0};
  uint8_t status;

  command.Instruction = W25Q_WRITE_ENABLE;
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  return HAL_QSPI_Command(&hqspi1, &command,
                          BOOT_QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         ReadStatus(&status) && (status & W25Q_STATUS_WEL) != 0U;
}

static bool WaitReady(uint32_t timeout_ms)
{
  const uint32_t start = HAL_GetTick();
  uint8_t status;

  do {
    if (!ReadStatus(&status)) {
      return false;
    }
    if ((status & W25Q_STATUS_BUSY) == 0U) {
      return true;
    }
    BootWatchdog_Refresh();
  } while ((HAL_GetTick() - start) < timeout_ms);

  return false;
}
