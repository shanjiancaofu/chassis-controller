#ifndef BOOT_OTA_METADATA_STORE_H
#define BOOT_OTA_METADATA_STORE_H

#include <stdbool.h>

#include "../../../shared/ota_metadata.h"

typedef enum
{
  BOOT_METADATA_COPY_A = 0,
  BOOT_METADATA_COPY_B
} BootMetadataCopy;

typedef enum
{
  BOOT_METADATA_OK = 0,
  BOOT_METADATA_ERROR_NULL,
  BOOT_METADATA_ERROR_MAGIC,
  BOOT_METADATA_ERROR_FORMAT,
  BOOT_METADATA_ERROR_SIZE,
  BOOT_METADATA_ERROR_CRC,
  BOOT_METADATA_ERROR_STATE,
  BOOT_METADATA_ERROR_SLOT,
  BOOT_METADATA_ERROR_IMAGE
} BootMetadataStatus;

typedef struct
{
  const OtaMetadata *metadata;
  BootMetadataCopy copy;
  bool valid;
} BootMetadataSelection;

typedef enum
{
  BOOT_INSTALL_ATTEMPT_READY = 0,
  BOOT_INSTALL_ROLLBACK_REQUIRED,
  BOOT_INSTALL_ATTEMPTS_EXHAUSTED
} BootInstallAttemptDecision;

BootMetadataStatus BootMetadataStore_Validate(const OtaMetadata *metadata);
BootMetadataSelection BootMetadataStore_SelectLatest(
    const OtaMetadata *copy_a, const OtaMetadata *copy_b);
bool BootMetadataStore_IsErased(const OtaMetadata *metadata);
BootInstallAttemptDecision BootMetadataStore_BeginInstallAttempt(
    OtaMetadata *metadata);

#endif
