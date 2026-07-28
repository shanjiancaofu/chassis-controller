#ifndef BOOT_METADATA_IO_H
#define BOOT_METADATA_IO_H

#include <stdbool.h>

#include "boot/ota_metadata_store.h"

typedef struct
{
  OtaMetadata copy_a;
  OtaMetadata copy_b;
  BootMetadataSelection selection;
} BootMetadataSnapshot;

bool BootMetadataIo_Load(BootMetadataSnapshot *snapshot);
bool BootMetadataIo_Commit(const BootMetadataSnapshot *current,
                           OtaMetadata *next);

#endif
