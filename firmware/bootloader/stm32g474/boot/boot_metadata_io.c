#include "boot/boot_metadata_io.h"

#include <stddef.h>
#include <string.h>

#include "bsp/qspi/boot_qspi_flash.h"
#include "components/crc/crc32.h"
#include "../../../shared/flash_layout.h"

bool BootMetadataIo_Load(BootMetadataSnapshot *snapshot)
{
  if (snapshot == NULL ||
      !BootQspiFlash_Read(OTA_QSPI_METADATA_A_START, &snapshot->copy_a,
                         sizeof(snapshot->copy_a)) ||
      !BootQspiFlash_Read(OTA_QSPI_METADATA_B_START, &snapshot->copy_b,
                         sizeof(snapshot->copy_b))) {
    return false;
  }

  snapshot->selection = BootMetadataStore_SelectLatest(
      &snapshot->copy_a, &snapshot->copy_b);
  return true;
}

bool BootMetadataIo_Commit(const BootMetadataSnapshot *current,
                           OtaMetadata *next)
{
  OtaMetadata verify;
  uint32_t target_address;

  if (current == NULL || next == NULL) {
    return false;
  }

  if (current->selection.valid) {
    next->sequence = current->selection.metadata->sequence + 1U;
    target_address =
        current->selection.copy == BOOT_METADATA_COPY_A
            ? OTA_QSPI_METADATA_B_START
            : OTA_QSPI_METADATA_A_START;
  } else {
    next->sequence = 1U;
    target_address = OTA_QSPI_METADATA_A_START;
  }

  next->record_crc32 =
      BootCrc32_Calculate(next, sizeof(*next) - sizeof(next->record_crc32));
  if (BootMetadataStore_Validate(next) != BOOT_METADATA_OK ||
      !BootQspiFlash_EraseSector(target_address) ||
      !BootQspiFlash_Program(target_address, next, sizeof(*next)) ||
      !BootQspiFlash_Read(target_address, &verify, sizeof(verify))) {
    return false;
  }

  return memcmp(next, &verify, sizeof(verify)) == 0 &&
         BootMetadataStore_Validate(&verify) == BOOT_METADATA_OK;
}
