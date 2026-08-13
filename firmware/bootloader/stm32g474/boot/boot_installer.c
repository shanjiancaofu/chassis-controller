#include "boot/boot_installer.h"

#include <stddef.h>

#include "boot/image_validator.h"
#include "bsp/flash/boot_internal_flash.h"
#include "bsp/qspi/boot_qspi_flash.h"
#include "components/crc/crc32.h"
#include "iwdg.h"
#include "../../../shared/firmware_image.h"
#include "../../../shared/flash_layout.h"

#define BOOT_INSTALL_CHUNK_SIZE 256U

static uint32_t SlotAddress(OtaSlotId slot);
static bool ValidatePayload(uint32_t payload_address,
                            const OtaImageHeader *header);
static bool ProgramPayload(uint32_t payload_address,
                           const OtaImageHeader *header);
static bool VerifyInstalledPayload(const OtaImageHeader *header);

BootInstallStatus BootInstaller_Install(const OtaMetadata *metadata,
                                        OtaSlotId slot)
{
  const uint32_t slot_address = SlotAddress(slot);
  OtaImageHeader header;
  uint32_t vectors[2];

  if (metadata == NULL || slot_address == 0U) {
    return BOOT_INSTALL_ERROR_SLOT;
  }
  if (!BootQspiFlash_Read(slot_address, &header, sizeof(header))) {
    return BOOT_INSTALL_ERROR_READ;
  }
  if (BootImageValidator_ValidateHeader(&header) != BOOT_IMAGE_OK) {
    return BOOT_INSTALL_ERROR_HEADER;
  }
  if (slot == metadata->candidate_slot &&
      (metadata->image_size != header.payload_size ||
       metadata->image_crc32 != header.payload_crc32)) {
    return BOOT_INSTALL_ERROR_METADATA;
  }
  if (!BootQspiFlash_Read(slot_address + sizeof(header), vectors,
                          sizeof(vectors)) ||
      !BootImageValidator_IsVectorTableValid(vectors, sizeof(vectors))) {
    return BOOT_INSTALL_ERROR_VECTOR;
  }
  if (!ValidatePayload(slot_address + sizeof(header), &header)) {
    return BOOT_INSTALL_ERROR_PAYLOAD_CRC;
  }
  if (!BootInternalFlash_IsLayoutSupported()) {
    return BOOT_INSTALL_ERROR_FLASH_LAYOUT;
  }
  if (!BootInternalFlash_EraseApplication()) {
    return BOOT_INSTALL_ERROR_ERASE;
  }
  if (!ProgramPayload(slot_address + sizeof(header), &header)) {
    return BOOT_INSTALL_ERROR_PROGRAM;
  }
  if (!VerifyInstalledPayload(&header)) {
    return BOOT_INSTALL_ERROR_VERIFY;
  }
  return BOOT_INSTALL_OK;
}

bool BootInstaller_VerifyInstalled(OtaSlotId slot)
{
  OtaImageHeader header;

  return BootInstaller_ReadImageHeader(slot, &header) &&
         VerifyInstalledPayload(&header);
}

bool BootInstaller_VerifyInstalledCandidate(const OtaMetadata *metadata)
{
  OtaImageHeader header;
  uint32_t slot_address;

  if (metadata == NULL || metadata->candidate_slot == OTA_SLOT_NONE) {
    return false;
  }
  slot_address = SlotAddress((OtaSlotId)metadata->candidate_slot);
  return slot_address != 0U &&
         BootInstaller_ReadImageHeader(
             (OtaSlotId)metadata->candidate_slot, &header) &&
         metadata->image_size == header.payload_size &&
         metadata->image_crc32 == header.payload_crc32 &&
         ValidatePayload(slot_address + sizeof(header), &header) &&
         VerifyInstalledPayload(&header);
}

bool BootInstaller_VerifyInstalledRecovery(OtaSlotId slot)
{
  const uint32_t slot_address = SlotAddress(slot);
  OtaImageHeader header;

  return slot_address != 0U &&
         BootInstaller_ReadImageHeader(slot, &header) &&
         ValidatePayload(slot_address + sizeof(header), &header) &&
         VerifyInstalledPayload(&header);
}

bool BootInstaller_ReadImageHeader(OtaSlotId slot, OtaImageHeader *header)
{
  const uint32_t slot_address = SlotAddress(slot);

  return header != NULL && slot_address != 0U &&
         BootQspiFlash_Read(slot_address, header, sizeof(*header)) &&
         BootImageValidator_ValidateHeader(header) == BOOT_IMAGE_OK;
}

static uint32_t SlotAddress(OtaSlotId slot)
{
  if (slot == OTA_SLOT_A) {
    return OTA_QSPI_SLOT_A_START;
  }
  if (slot == OTA_SLOT_B) {
    return OTA_QSPI_SLOT_B_START;
  }
  return 0U;
}

static bool ValidatePayload(uint32_t payload_address,
                            const OtaImageHeader *header)
{
  uint8_t buffer[BOOT_INSTALL_CHUNK_SIZE];
  BootCrc32Context crc;
  uint32_t offset = 0U;

  BootCrc32_Init(&crc);
  while (offset < header->payload_size) {
    const uint32_t remaining = header->payload_size - offset;
    const uint32_t chunk = remaining < sizeof(buffer)
                               ? remaining
                               : sizeof(buffer);

    if (!BootQspiFlash_Read(payload_address + offset, buffer, chunk)) {
      return false;
    }
    BootCrc32_Update(&crc, buffer, chunk);
    offset += chunk;
    BootWatchdog_Refresh();
  }
  return BootCrc32_Finalize(&crc) == header->payload_crc32;
}

static bool ProgramPayload(uint32_t payload_address,
                           const OtaImageHeader *header)
{
  uint8_t buffer[BOOT_INSTALL_CHUNK_SIZE];
  uint32_t offset = 0U;

  while (offset < header->payload_size) {
    const uint32_t remaining = header->payload_size - offset;
    const uint32_t chunk = remaining < sizeof(buffer)
                               ? remaining
                               : sizeof(buffer);

    if (!BootQspiFlash_Read(payload_address + offset, buffer, chunk) ||
        !BootInternalFlash_Program(OTA_APPLICATION_START + offset,
                                   buffer, chunk)) {
      return false;
    }
    offset += chunk;
    BootWatchdog_Refresh();
  }
  return true;
}

static bool VerifyInstalledPayload(const OtaImageHeader *header)
{
  BootCrc32Context crc;
  uint32_t offset = 0U;

  BootCrc32_Init(&crc);
  while (offset < header->payload_size) {
    const uint32_t remaining = header->payload_size - offset;
    const uint32_t chunk = remaining < BOOT_INSTALL_CHUNK_SIZE
                               ? remaining
                               : BOOT_INSTALL_CHUNK_SIZE;

    BootCrc32_Update(&crc,
                     (const void *)(OTA_APPLICATION_START + offset),
                     chunk);
    offset += chunk;
    BootWatchdog_Refresh();
  }
  return BootCrc32_Finalize(&crc) == header->payload_crc32 &&
         BootImageValidator_IsVectorTableValid(
             (const void *)OTA_APPLICATION_START, header->payload_size);
}
