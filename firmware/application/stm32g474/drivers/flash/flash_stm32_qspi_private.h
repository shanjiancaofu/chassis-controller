#ifndef CHASSIS_FLASH_STM32_QSPI_BACKEND_H
#define CHASSIS_FLASH_STM32_QSPI_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  FLASH_STM32_QSPI_TRANSFER_IDLE = 0,
  FLASH_STM32_QSPI_TRANSFER_BUSY,
  FLASH_STM32_QSPI_TRANSFER_COMPLETE,
  FLASH_STM32_QSPI_TRANSFER_FAILED
} FlashStm32QspiTransferStatus;

bool FlashStm32QspiReadJedecId(uint8_t jedec_id[3]);
bool FlashStm32QspiRead(uint32_t address, uint8_t *data, uint32_t size);
bool FlashStm32QspiReadDma(uint32_t address, uint8_t *data,
                          uint32_t size);
bool FlashStm32QspiEraseSector(uint32_t address);
bool FlashStm32QspiProgramPageDma(uint32_t address,
                                 const uint8_t *data,
                                 uint32_t size);
bool FlashStm32QspiIsBusy(bool *busy);
FlashStm32QspiTransferStatus FlashStm32QspiGetTransferStatus(void);
void FlashStm32QspiAbort(void);

#endif
