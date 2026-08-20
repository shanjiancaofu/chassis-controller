#include "drivers/flash.h"

#include "bsp/qspi/bsp_qspi_flash.h"

bool flash_read_jedec_id(uint8_t jedec_id[3])
{
  return BspQspiFlash_ReadJedecId(jedec_id);
}

bool flash_read(uint32_t address, uint8_t *data, uint32_t size)
{
  return BspQspiFlash_Read(address, data, size);
}

bool flash_read_dma(uint32_t address, uint8_t *data, uint32_t size)
{
  return BspQspiFlash_ReadDma(address, data, size);
}

bool flash_erase_sector(uint32_t address)
{
  return BspQspiFlash_EraseSector(address);
}

bool flash_program_page_dma(uint32_t address, const uint8_t *data, uint32_t size)
{
  return BspQspiFlash_ProgramPageDma(address, data, size);
}

bool flash_is_busy(bool *busy)
{
  return BspQspiFlash_IsBusy(busy);
}

FlashTransferStatus flash_get_transfer_status(void)
{
  switch (BspQspiFlash_GetTransferStatus()) {
    case BSP_QSPI_TRANSFER_BUSY:
      return FLASH_TRANSFER_BUSY;
    case BSP_QSPI_TRANSFER_COMPLETE:
      return FLASH_TRANSFER_COMPLETE;
    case BSP_QSPI_TRANSFER_FAILED:
      return FLASH_TRANSFER_FAILED;
    case BSP_QSPI_TRANSFER_IDLE:
    default:
      return FLASH_TRANSFER_IDLE;
  }
}

void flash_abort(void)
{
  BspQspiFlash_Abort();
}
