#ifndef BSP_QSPI_FLASH_H
#define BSP_QSPI_FLASH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  BSP_QSPI_TRANSFER_IDLE = 0,
  BSP_QSPI_TRANSFER_BUSY,
  BSP_QSPI_TRANSFER_COMPLETE,
  BSP_QSPI_TRANSFER_FAILED
} BspQspiTransferStatus;

bool BspQspiFlash_ReadDma(uint32_t address, uint8_t *data,
                          uint32_t size);
bool BspQspiFlash_EraseSector(uint32_t address);
bool BspQspiFlash_ProgramPageDma(uint32_t address,
                                 const uint8_t *data,
                                 uint32_t size);
bool BspQspiFlash_IsBusy(bool *busy);
BspQspiTransferStatus BspQspiFlash_GetTransferStatus(void);

#endif
