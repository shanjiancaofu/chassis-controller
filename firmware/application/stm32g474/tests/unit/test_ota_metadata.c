#ifdef OTA_METADATA_HOST_TEST

#include <assert.h>
#include <string.h>

#include "communication/ota_transport/ota_metadata.h"

int main(void)
{
  OtaMetadata metadata;

  assert(OtaMetadata_IsReplaceableState(OTA_STATE_EMPTY));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_RECEIVING));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_STAGED));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_CONFIRMED));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_FAILED));

  assert(!OtaMetadata_IsReplaceableState(OTA_STATE_INSTALLING));
  assert(!OtaMetadata_IsReplaceableState(OTA_STATE_TRIAL));
  assert(!OtaMetadata_IsReplaceableState(OTA_STATE_ROLLBACK_PENDING));

  memset(&metadata, 0, sizeof(metadata));
  metadata.magic = OTA_METADATA_MAGIC;
  metadata.format_version = OTA_METADATA_FORMAT_VERSION;
  metadata.record_size = sizeof(metadata);
  metadata.state = OTA_STATE_EMPTY;
  metadata.confirmed_slot = OTA_SLOT_NONE;
  metadata.candidate_slot = OTA_SLOT_NONE;
  OtaMetadata_UpdateCrc(&metadata);
  assert(OtaMetadata_Validate(&metadata));
  metadata.candidate_slot = OTA_SLOT_A;
  OtaMetadata_UpdateCrc(&metadata);
  assert(!OtaMetadata_Validate(&metadata));

  metadata.state = OTA_STATE_RECEIVING;
  metadata.confirmed_slot = OTA_SLOT_NONE;
  metadata.candidate_slot = OTA_SLOT_A;
  OtaMetadata_UpdateCrc(&metadata);
  assert(OtaMetadata_Validate(&metadata));
  metadata.candidate_slot = OTA_SLOT_NONE;
  OtaMetadata_UpdateCrc(&metadata);
  assert(!OtaMetadata_Validate(&metadata));
  return 0;
}

#endif
