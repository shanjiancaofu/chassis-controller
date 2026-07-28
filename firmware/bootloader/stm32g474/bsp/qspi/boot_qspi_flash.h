#ifndef BOOT_QSPI_FLASH_H
#define BOOT_QSPI_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#define BOOT_QSPI_CAPACITY_BYTES 0x00800000UL
#define BOOT_QSPI_SECTOR_SIZE 0x00001000UL
#define BOOT_QSPI_PAGE_SIZE 256UL

bool BootQspiFlash_ReadJedecId(uint8_t jedec_id[3]);
bool BootQspiFlash_Read(uint32_t address, void *data, uint32_t size);
bool BootQspiFlash_EraseSector(uint32_t address);
bool BootQspiFlash_Program(uint32_t address, const void *data,
                           uint32_t size);

#endif
