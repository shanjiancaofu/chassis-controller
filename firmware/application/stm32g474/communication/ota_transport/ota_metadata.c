#include "communication/ota_transport/ota_metadata.h"

#include <stddef.h>

#include "components/crc/crc32.h"
#include "../../../../shared/flash_layout.h"

static bool SlotsAreValid(const OtaMetadata *metadata);

bool OtaMetadata_Validate(const OtaMetadata *metadata)
{
  if (metadata == NULL || metadata->magic != OTA_METADATA_MAGIC ||
      metadata->format_version != OTA_METADATA_FORMAT_VERSION ||
      metadata->record_size != sizeof(OtaMetadata) ||
      metadata->state > OTA_STATE_FAILED ||
      metadata->image_size > OTA_APPLICATION_SIZE ||
      !SlotsAreValid(metadata)) {
    return false;
  }
  return Crc32_Calculate(
             metadata,
             sizeof(*metadata) - sizeof(metadata->record_crc32)) ==
         metadata->record_crc32;
}

OtaMetadataSelection OtaMetadata_SelectLatest(
    const OtaMetadata *copy_a, const OtaMetadata *copy_b)
{
  const bool valid_a = OtaMetadata_Validate(copy_a);
  const bool valid_b = OtaMetadata_Validate(copy_b);
  OtaMetadataSelection selection = {0};

  if (valid_a && valid_b) {
    if ((int32_t)(copy_a->sequence - copy_b->sequence) >= 0) {
      selection.metadata = copy_a;
      selection.copy = OTA_METADATA_COPY_A;
    } else {
      selection.metadata = copy_b;
      selection.copy = OTA_METADATA_COPY_B;
    }
    selection.valid = true;
  } else if (valid_a) {
    selection.metadata = copy_a;
    selection.copy = OTA_METADATA_COPY_A;
    selection.valid = true;
  } else if (valid_b) {
    selection.metadata = copy_b;
    selection.copy = OTA_METADATA_COPY_B;
    selection.valid = true;
  }
  return selection;
}

void OtaMetadata_UpdateCrc(OtaMetadata *metadata)
{
  if (metadata != NULL) {
    metadata->record_crc32 = Crc32_Calculate(
        metadata,
        sizeof(*metadata) - sizeof(metadata->record_crc32));
  }
}

static bool SlotsAreValid(const OtaMetadata *metadata)
{
  const bool confirmed_valid =
      metadata->confirmed_slot == OTA_SLOT_A ||
      metadata->confirmed_slot == OTA_SLOT_B;
  const bool candidate_valid =
      metadata->candidate_slot == OTA_SLOT_A ||
      metadata->candidate_slot == OTA_SLOT_B;
  const bool confirmed_optional =
      confirmed_valid || metadata->confirmed_slot == OTA_SLOT_NONE;
  const bool candidate_optional =
      candidate_valid || metadata->candidate_slot == OTA_SLOT_NONE;

  switch ((OtaState)metadata->state) {
    case OTA_STATE_EMPTY:
    case OTA_STATE_RECEIVING:
    case OTA_STATE_FAILED:
      return confirmed_optional && candidate_optional;
    case OTA_STATE_CONFIRMED:
      return confirmed_valid && candidate_optional;
    case OTA_STATE_STAGED:
    case OTA_STATE_INSTALLING:
    case OTA_STATE_TRIAL:
      return confirmed_optional && candidate_valid &&
             metadata->confirmed_slot != metadata->candidate_slot;
    case OTA_STATE_ROLLBACK_PENDING:
      return confirmed_valid && candidate_valid &&
             metadata->confirmed_slot != metadata->candidate_slot;
    default:
      return false;
  }
}
