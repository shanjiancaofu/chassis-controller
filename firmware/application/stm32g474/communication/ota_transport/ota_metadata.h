#ifndef OTA_METADATA_STORE_H
#define OTA_METADATA_STORE_H

#include <stdbool.h>

#include "../../../../shared/ota_metadata.h"

typedef enum
{
  OTA_METADATA_COPY_A = 0,
  OTA_METADATA_COPY_B
} OtaMetadataCopy;

typedef struct
{
  const OtaMetadata *metadata;
  OtaMetadataCopy copy;
  bool valid;
} OtaMetadataSelection;

bool OtaMetadata_Validate(const OtaMetadata *metadata);
OtaMetadataSelection OtaMetadata_SelectLatest(
    const OtaMetadata *copy_a, const OtaMetadata *copy_b);
void OtaMetadata_UpdateCrc(OtaMetadata *metadata);

#endif
