#ifndef BOOT_INSTALL_RECOVERY_H
#define BOOT_INSTALL_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../shared/ota_metadata.h"

#define BOOT_INSTALL_ATTEMPT_LIMIT 3U

typedef enum
{
  BOOT_INSTALL_RECOVERY_BEGIN = 0,
  BOOT_INSTALL_RECOVERY_SALVAGE,
  BOOT_INSTALL_RECOVERY_ROLLBACK,
  BOOT_INSTALL_RECOVERY_FAILED
} BootInstallRecoveryAction;

static inline BootInstallRecoveryAction
BootInstallRecovery_DecideCandidate(const OtaMetadata *metadata,
                                    bool installed_candidate_valid)
{
  if (metadata == 0) {
    return BOOT_INSTALL_RECOVERY_FAILED;
  }
  if (metadata->state == OTA_STATE_INSTALLING &&
      metadata->install_attempts > 0U && installed_candidate_valid) {
    return BOOT_INSTALL_RECOVERY_SALVAGE;
  }
  if (metadata->install_attempts < BOOT_INSTALL_ATTEMPT_LIMIT) {
    return BOOT_INSTALL_RECOVERY_BEGIN;
  }
  return metadata->confirmed_slot != OTA_SLOT_NONE
             ? BOOT_INSTALL_RECOVERY_ROLLBACK
             : BOOT_INSTALL_RECOVERY_FAILED;
}

static inline BootInstallRecoveryAction
BootInstallRecovery_DecideConfirmed(const OtaMetadata *metadata,
                                    bool installed_confirmed_valid)
{
  if (metadata == 0) {
    return BOOT_INSTALL_RECOVERY_FAILED;
  }
  if (metadata->state == OTA_STATE_ROLLBACK_PENDING &&
      installed_confirmed_valid) {
    return BOOT_INSTALL_RECOVERY_SALVAGE;
  }
  return metadata->install_attempts < BOOT_INSTALL_ATTEMPT_LIMIT
             ? BOOT_INSTALL_RECOVERY_BEGIN
             : BOOT_INSTALL_RECOVERY_FAILED;
}

static inline void BootInstallRecovery_MarkTrial(OtaMetadata *metadata)
{
  if (metadata == 0) {
    return;
  }
  metadata->state = OTA_STATE_TRIAL;
  metadata->install_attempts = 0U;
  metadata->trial_boot_count = 0U;
  metadata->last_error = 0U;
}

static inline void BootInstallRecovery_MarkConfirmed(
    OtaMetadata *metadata, uint32_t image_size, uint32_t image_crc32)
{
  if (metadata == 0) {
    return;
  }
  metadata->state = OTA_STATE_CONFIRMED;
  metadata->candidate_slot = OTA_SLOT_NONE;
  metadata->image_size = image_size;
  metadata->image_crc32 = image_crc32;
  metadata->install_attempts = 0U;
  metadata->trial_boot_count = 0U;
  metadata->last_error = 0U;
}

#endif
