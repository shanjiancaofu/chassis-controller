#ifndef BOOT_INTERNAL_FLASH_H
#define BOOT_INTERNAL_FLASH_H

#include <stdbool.h>
#include <stdint.h>

bool BootInternalFlash_IsLayoutSupported(void);
bool BootInternalFlash_EraseApplication(void);
bool BootInternalFlash_Program(uint32_t address, const void *data,
                               uint32_t size);
uint32_t BootInternalFlash_GetLastEraseBank(void);
uint32_t BootInternalFlash_GetLastErasePage(void);
uint32_t BootInternalFlash_GetLastError(void);

#endif
