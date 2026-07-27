#ifndef BOOT_IMAGE_VALIDATOR_H
#define BOOT_IMAGE_VALIDATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../shared/firmware_image.h"

typedef enum
{
  BOOT_IMAGE_OK = 0,
  BOOT_IMAGE_ERROR_NULL,
  BOOT_IMAGE_ERROR_MAGIC,
  BOOT_IMAGE_ERROR_FORMAT,
  BOOT_IMAGE_ERROR_HEADER_SIZE,
  BOOT_IMAGE_ERROR_HEADER_CRC,
  BOOT_IMAGE_ERROR_FLAGS,
  BOOT_IMAGE_ERROR_ADDRESS,
  BOOT_IMAGE_ERROR_SIZE,
  BOOT_IMAGE_ERROR_PAYLOAD_CRC,
  BOOT_IMAGE_ERROR_VECTOR
} BootImageStatus;

BootImageStatus BootImageValidator_ValidateHeader(
    const OtaImageHeader *header);
BootImageStatus BootImageValidator_ValidatePackage(
    const void *package, size_t package_size);
bool BootImageValidator_IsVectorTableValid(const void *payload,
                                           size_t payload_size);

#endif
