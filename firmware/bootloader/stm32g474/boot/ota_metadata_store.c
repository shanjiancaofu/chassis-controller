#include "boot/ota_metadata_store.h"

#include <stddef.h>

#include "components/crc/crc32.h"
#include "../../../shared/flash_layout.h"

#define BOOT_INSTALL_ATTEMPT_LIMIT 3U

static bool IsKnownState(uint32_t state);
static bool IsKnownSlot(uint32_t slot, bool allow_none);
static bool HasValidSlotsForState(const OtaMetadata *metadata);

BootMetadataStatus BootMetadataStore_Validate(const OtaMetadata *metadata)
{
  uint32_t calculated_crc;

  if (metadata == 0) {
    return BOOT_METADATA_ERROR_NULL;
  }
  if (metadata->magic != OTA_METADATA_MAGIC) {
    return BOOT_METADATA_ERROR_MAGIC;
  }
  if (metadata->format_version != OTA_METADATA_FORMAT_VERSION) {
    return BOOT_METADATA_ERROR_FORMAT;
  }
  if (metadata->record_size != sizeof(OtaMetadata)) {
    return BOOT_METADATA_ERROR_SIZE;
  }

  calculated_crc = BootCrc32_Calculate(metadata, sizeof(OtaMetadata) - 4U);
  if (calculated_crc != metadata->record_crc32) {
    return BOOT_METADATA_ERROR_CRC;
  }
  if (!IsKnownState(metadata->state)) {
    return BOOT_METADATA_ERROR_STATE;
  }
  if (!HasValidSlotsForState(metadata)) {
    return BOOT_METADATA_ERROR_SLOT;
  }
  if (metadata->image_size > OTA_APPLICATION_SIZE) {
    return BOOT_METADATA_ERROR_IMAGE;
  }

  return BOOT_METADATA_OK;
}

BootMetadataSelection BootMetadataStore_SelectLatest(
    const OtaMetadata *copy_a, const OtaMetadata *copy_b)
{
  const bool valid_a =
      BootMetadataStore_Validate(copy_a) == BOOT_METADATA_OK;
  const bool valid_b =
      BootMetadataStore_Validate(copy_b) == BOOT_METADATA_OK;
  BootMetadataSelection selection = {0};

  if (valid_a && valid_b) {
    if ((int32_t)(copy_a->sequence - copy_b->sequence) >= 0) {
      selection.metadata = copy_a;
      selection.copy = BOOT_METADATA_COPY_A;
    } else {
      selection.metadata = copy_b;
      selection.copy = BOOT_METADATA_COPY_B;
    }
    selection.valid = true;
  } else if (valid_a) {
    selection.metadata = copy_a;
    selection.copy = BOOT_METADATA_COPY_A;
    selection.valid = true;
  } else if (valid_b) {
    selection.metadata = copy_b;
    selection.copy = BOOT_METADATA_COPY_B;
    selection.valid = true;
  }

  return selection;
}

bool BootMetadataStore_IsErased(const OtaMetadata *metadata)
{
  const uint8_t *bytes = (const uint8_t *)metadata;
  size_t index;

  if (metadata == 0) {
    return false;
  }
  for (index = 0U; index < sizeof(*metadata); ++index) {
    if (bytes[index] != 0xFFU) {
      return false;
    }
  }
  return true;
}

BootInstallAttemptDecision BootMetadataStore_BeginInstallAttempt(
    OtaMetadata *metadata)
{
  if (metadata == 0 ||
      metadata->install_attempts >= BOOT_INSTALL_ATTEMPT_LIMIT) {
    if (metadata != 0 && metadata->confirmed_slot != OTA_SLOT_NONE) {
      metadata->state = OTA_STATE_ROLLBACK_PENDING;
      return BOOT_INSTALL_ROLLBACK_REQUIRED;
    }
    if (metadata != 0) {
      metadata->state = OTA_STATE_FAILED;
    }
    return BOOT_INSTALL_ATTEMPTS_EXHAUSTED;
  }

  metadata->state = OTA_STATE_INSTALLING;
  ++metadata->install_attempts;
  return BOOT_INSTALL_ATTEMPT_READY;
}

static bool IsKnownState(uint32_t state)
{
  return state <= OTA_STATE_FAILED;
}

static bool IsKnownSlot(uint32_t slot, bool allow_none)
{
  if (allow_none && slot == OTA_SLOT_NONE) {
    return true;
  }
  return slot == OTA_SLOT_A || slot == OTA_SLOT_B;
}

static bool HasValidSlotsForState(const OtaMetadata *metadata)
{
  const bool has_confirmed = IsKnownSlot(metadata->confirmed_slot, false);
  const bool has_candidate = IsKnownSlot(metadata->candidate_slot, false);
  const bool confirmed_is_empty = metadata->confirmed_slot == OTA_SLOT_NONE;

  switch (metadata->state) {
    case OTA_STATE_EMPTY:
    case OTA_STATE_RECEIVING:
    case OTA_STATE_FAILED:
      return IsKnownSlot(metadata->confirmed_slot, true) &&
             IsKnownSlot(metadata->candidate_slot, true);
    case OTA_STATE_CONFIRMED:
      return has_confirmed &&
             IsKnownSlot(metadata->candidate_slot, true);
    case OTA_STATE_STAGED:
    case OTA_STATE_INSTALLING:
    case OTA_STATE_TRIAL:
      return (has_confirmed || confirmed_is_empty) && has_candidate &&
             metadata->confirmed_slot != metadata->candidate_slot;
    case OTA_STATE_ROLLBACK_PENDING:
      return has_confirmed && has_candidate &&
             metadata->confirmed_slot != metadata->candidate_slot;
    default:
      return false;
  }
}
