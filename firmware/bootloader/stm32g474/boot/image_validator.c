#include "boot/image_validator.h"

#include "components/crc/crc32.h"
#include "../../../shared/flash_layout.h"

#define BOOT_SRAM_START 0x20000000UL
#define BOOT_SRAM_END 0x20020000UL

BootImageStatus BootImageValidator_ValidateHeader(
    const OtaImageHeader *header)
{
  uint32_t calculated_crc;

  if (header == 0) {
    return BOOT_IMAGE_ERROR_NULL;
  }
  if (header->magic != OTA_IMAGE_MAGIC) {
    return BOOT_IMAGE_ERROR_MAGIC;
  }
  if (header->format_version != OTA_IMAGE_FORMAT_VERSION) {
    return BOOT_IMAGE_ERROR_FORMAT;
  }
  if (header->header_size != sizeof(OtaImageHeader)) {
    return BOOT_IMAGE_ERROR_HEADER_SIZE;
  }

  calculated_crc = BootCrc32_Calculate(header, sizeof(OtaImageHeader) - 4U);
  if (calculated_crc != header->header_crc32) {
    return BOOT_IMAGE_ERROR_HEADER_CRC;
  }
  if (header->flags != OTA_IMAGE_FLAG_NONE) {
    return BOOT_IMAGE_ERROR_FLAGS;
  }
  if (header->load_address != OTA_APPLICATION_START ||
      header->vector_address != OTA_APPLICATION_START) {
    return BOOT_IMAGE_ERROR_ADDRESS;
  }
  if (header->payload_size == 0U ||
      header->payload_size > OTA_APPLICATION_SIZE) {
    return BOOT_IMAGE_ERROR_SIZE;
  }

  return BOOT_IMAGE_OK;
}

BootImageStatus BootImageValidator_ValidatePackage(
    const void *package, size_t package_size)
{
  const uint8_t *bytes = (const uint8_t *)package;
  const OtaImageHeader *header = (const OtaImageHeader *)package;
  const uint8_t *payload;
  BootImageStatus status;

  if (package == 0) {
    return BOOT_IMAGE_ERROR_NULL;
  }
  if (package_size < sizeof(OtaImageHeader)) {
    return BOOT_IMAGE_ERROR_SIZE;
  }

  status = BootImageValidator_ValidateHeader(header);
  if (status != BOOT_IMAGE_OK) {
    return status;
  }
  if (package_size != sizeof(OtaImageHeader) + header->payload_size) {
    return BOOT_IMAGE_ERROR_SIZE;
  }

  payload = &bytes[sizeof(OtaImageHeader)];
  if (BootCrc32_Calculate(payload, header->payload_size) !=
      header->payload_crc32) {
    return BOOT_IMAGE_ERROR_PAYLOAD_CRC;
  }
  if (!BootImageValidator_IsVectorTableValid(payload, header->payload_size)) {
    return BOOT_IMAGE_ERROR_VECTOR;
  }

  return BOOT_IMAGE_OK;
}

bool BootImageValidator_IsVectorTableValid(const void *payload,
                                           size_t payload_size)
{
  const uint32_t *vector = (const uint32_t *)payload;
  uint32_t initial_sp;
  uint32_t reset_handler;
  uint32_t reset_address;

  if (payload == 0 || payload_size < 8U) {
    return false;
  }

  initial_sp = vector[0];
  reset_handler = vector[1];
  reset_address = reset_handler & ~1UL;

  if (initial_sp < BOOT_SRAM_START || initial_sp > BOOT_SRAM_END ||
      (initial_sp % 8UL) != 0UL) {
    return false;
  }
  if ((reset_handler & 1UL) == 0UL) {
    return false;
  }
  if (reset_address < OTA_APPLICATION_START ||
      reset_address >= OTA_APPLICATION_START + OTA_APPLICATION_SIZE) {
    return false;
  }

  return true;
}
