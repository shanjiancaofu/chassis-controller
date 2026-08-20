#include "drivers/flash.h"

#include "drivers/flash/flash_stm32_qspi_private.h"
#include "device.h"
#include "devicetree_generated.h"

static bool Ready(void)
{
  return device_is_ready(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_FLASH));
}

bool flash_read_jedec_id(uint8_t jedec_id[3])
{
  return Ready() && FlashStm32QspiReadJedecId(jedec_id);
}

bool flash_read(uint32_t address, uint8_t *data, uint32_t size)
{
  return Ready() && FlashStm32QspiRead(address, data, size);
}

bool flash_read_dma(uint32_t address, uint8_t *data, uint32_t size)
{
  return Ready() && FlashStm32QspiReadDma(address, data, size);
}

bool flash_erase_sector(uint32_t address)
{
  return Ready() && FlashStm32QspiEraseSector(address);
}

bool flash_program_page_dma(uint32_t address, const uint8_t *data, uint32_t size)
{
  return Ready() && FlashStm32QspiProgramPageDma(address, data, size);
}

bool flash_is_busy(bool *busy)
{
  return Ready() && FlashStm32QspiIsBusy(busy);
}

FlashTransferStatus flash_get_transfer_status(void)
{
  if (!Ready()) {
    return FLASH_TRANSFER_FAILED;
  }
  switch (FlashStm32QspiGetTransferStatus()) {
    case FLASH_STM32_QSPI_TRANSFER_BUSY:
      return FLASH_TRANSFER_BUSY;
    case FLASH_STM32_QSPI_TRANSFER_COMPLETE:
      return FLASH_TRANSFER_COMPLETE;
    case FLASH_STM32_QSPI_TRANSFER_FAILED:
      return FLASH_TRANSFER_FAILED;
    case FLASH_STM32_QSPI_TRANSFER_IDLE:
    default:
      return FLASH_TRANSFER_IDLE;
  }
}

void flash_abort(void)
{
  if (Ready()) {
    FlashStm32QspiAbort();
  }
}
