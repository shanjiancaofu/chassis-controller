#ifdef OTA_METADATA_HOST_TEST

#include <assert.h>

#include "communication/ota_transport/ota_metadata.h"

int main(void)
{
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_EMPTY));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_RECEIVING));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_STAGED));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_CONFIRMED));
  assert(OtaMetadata_IsReplaceableState(OTA_STATE_FAILED));

  assert(!OtaMetadata_IsReplaceableState(OTA_STATE_INSTALLING));
  assert(!OtaMetadata_IsReplaceableState(OTA_STATE_TRIAL));
  assert(!OtaMetadata_IsReplaceableState(OTA_STATE_ROLLBACK_PENDING));
  return 0;
}

#endif
