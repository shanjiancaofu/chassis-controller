#ifndef QSPI_FLASH_IDENTITY_H
#define QSPI_FLASH_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

#define QSPI_FLASH_JEDEC_MANUFACTURER_ID 0xEFU
#define QSPI_FLASH_JEDEC_MEMORY_TYPE 0x40U
#define QSPI_FLASH_JEDEC_CAPACITY_ID 0x17U
#define QSPI_FLASH_CAPACITY_BYTES 0x00800000UL
#define QSPI_FLASH_SECTOR_SIZE 0x00001000UL
#define QSPI_FLASH_PAGE_SIZE 0x00000100UL

static inline bool QspiFlashIdentity_IsSupported(const uint8_t jedec_id[3])
{
  return jedec_id != 0 &&
         jedec_id[0] == QSPI_FLASH_JEDEC_MANUFACTURER_ID &&
         jedec_id[1] == QSPI_FLASH_JEDEC_MEMORY_TYPE &&
         jedec_id[2] == QSPI_FLASH_JEDEC_CAPACITY_ID;
}

#endif
