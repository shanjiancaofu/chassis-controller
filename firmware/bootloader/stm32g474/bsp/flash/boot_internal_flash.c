#include "bsp/flash/boot_internal_flash.h"

#include <string.h>

#include "iwdg.h"
#include "main.h"
#include "../../../../shared/flash_layout.h"

#define BOOT_FLASH_BANK_SIZE 0x00040000UL
#define BOOT_FLASH_PAGE_SIZE 0x00000800UL
#define BOOT_FLASH_DOUBLEWORD_SIZE 8U

static bool ErasePages(uint32_t bank, uint32_t first_page,
                       uint32_t page_count);

bool BootInternalFlash_IsLayoutSupported(void)
{
  return (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U) &&
         (OTA_APPLICATION_START % BOOT_FLASH_PAGE_SIZE) == 0U &&
         (OTA_APPLICATION_SIZE % BOOT_FLASH_PAGE_SIZE) == 0U &&
         OTA_APPLICATION_START + OTA_APPLICATION_SIZE ==
             OTA_BOOTLOADER_START + 0x00080000UL;
}

bool BootInternalFlash_EraseApplication(void)
{
  const uint32_t bank1_first_page =
      (OTA_APPLICATION_START - OTA_BOOTLOADER_START) /
      BOOT_FLASH_PAGE_SIZE;
  const uint32_t pages_per_bank =
      BOOT_FLASH_BANK_SIZE / BOOT_FLASH_PAGE_SIZE;
  const uint32_t bank1_page_count = pages_per_bank - bank1_first_page;
  bool success;

  if (!BootInternalFlash_IsLayoutSupported() ||
      HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  success = ErasePages(FLASH_BANK_1, bank1_first_page,
                       bank1_page_count) &&
            ErasePages(FLASH_BANK_2, 0U, pages_per_bank);

  if (HAL_FLASH_Lock() != HAL_OK) {
    success = false;
  }
  return success;
}

bool BootInternalFlash_Program(uint32_t address, const void *data,
                               uint32_t size)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t remaining = size;
  bool success = true;

  if (data == NULL || size == 0U ||
      (address % BOOT_FLASH_DOUBLEWORD_SIZE) != 0U ||
      address < OTA_APPLICATION_START ||
      address >= OTA_APPLICATION_START + OTA_APPLICATION_SIZE ||
      size > OTA_APPLICATION_START + OTA_APPLICATION_SIZE - address ||
      HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  while (remaining > 0U) {
    uint64_t value = UINT64_MAX;
    const uint32_t chunk =
        remaining < BOOT_FLASH_DOUBLEWORD_SIZE
            ? remaining
            : BOOT_FLASH_DOUBLEWORD_SIZE;

    memcpy(&value, bytes, chunk);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address,
                          value) != HAL_OK ||
        memcmp((const void *)address, bytes, chunk) != 0) {
      success = false;
      break;
    }

    address += BOOT_FLASH_DOUBLEWORD_SIZE;
    bytes += chunk;
    remaining -= chunk;
    BootWatchdog_Refresh();
  }

  if (HAL_FLASH_Lock() != HAL_OK) {
    success = false;
  }
  return success;
}

static bool ErasePages(uint32_t bank, uint32_t first_page,
                       uint32_t page_count)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0U;

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks = bank;
  erase.Page = first_page;
  erase.NbPages = page_count;

  BootWatchdog_Refresh();
  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK ||
      page_error != 0xFFFFFFFFUL) {
    return false;
  }
  BootWatchdog_Refresh();
  return true;
}
