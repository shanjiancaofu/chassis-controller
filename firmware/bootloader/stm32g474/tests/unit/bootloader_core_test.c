#include <stdio.h>
#include <string.h>

#include "boot/boot_install_recovery.h"
#include "boot/boot_installer.h"
#include "boot/image_validator.h"
#include "boot/ota_metadata_store.h"
#include "components/crc/crc32.h"
#include "../../../../shared/flash_layout.h"

static int failures;

static void Expect(int condition, const char *name)
{
  if (!condition) {
    printf("FAIL %s\n", name);
    ++failures;
  }
}

static OtaImageHeader MakeHeader(uint8_t *payload, uint32_t payload_size)
{
  OtaImageHeader header;

  memset(&header, 0, sizeof(header));
  header.magic = OTA_IMAGE_MAGIC;
  header.format_version = OTA_IMAGE_FORMAT_VERSION;
  header.header_size = sizeof(OtaImageHeader);
  header.payload_size = payload_size;
  header.payload_crc32 = BootCrc32_Calculate(payload, payload_size);
  header.load_address = OTA_APPLICATION_START;
  header.vector_address = OTA_APPLICATION_START;
  header.firmware_version = 0x00010002UL;
  header.build_number = 7UL;
  header.flags = OTA_IMAGE_FLAG_NONE;
  header.header_crc32 =
      BootCrc32_Calculate(&header, sizeof(OtaImageHeader) - 4U);
  return header;
}

static OtaMetadata MakeMetadata(uint32_t sequence, uint32_t state)
{
  OtaMetadata metadata;

  memset(&metadata, 0, sizeof(metadata));
  metadata.magic = OTA_METADATA_MAGIC;
  metadata.format_version = OTA_METADATA_FORMAT_VERSION;
  metadata.record_size = sizeof(OtaMetadata);
  metadata.sequence = sequence;
  metadata.state = state;
  metadata.confirmed_slot = OTA_SLOT_A;
  metadata.candidate_slot = state == OTA_STATE_CONFIRMED ? OTA_SLOT_NONE
                                                         : OTA_SLOT_B;
  metadata.image_size = 1024U;
  metadata.image_crc32 = 0x12345678UL;
  metadata.record_crc32 =
      BootCrc32_Calculate(&metadata, sizeof(OtaMetadata) - 4U);
  return metadata;
}

static void TestCrc32(void)
{
  static const char test[] = "123456789";

  Expect(BootCrc32_Calculate(test, strlen(test)) == 0xCBF43926UL,
         "crc32_known_vector");
}

static void TestImageValidation(void)
{
  uint8_t package[sizeof(OtaImageHeader) + 32U];
  uint8_t *payload = &package[sizeof(OtaImageHeader)];
  OtaImageHeader header;

  memset(package, 0, sizeof(package));
  ((uint32_t *)payload)[0] = 0x20001000UL;
  ((uint32_t *)payload)[1] = OTA_APPLICATION_START + 1UL;

  header = MakeHeader(payload, 32U);
  memcpy(package, &header, sizeof(header));
  Expect(BootImageValidator_ValidatePackage(package, sizeof(package)) ==
             BOOT_IMAGE_OK,
         "image_package_valid");

  package[sizeof(package) - 1U] ^= 0x55U;
  Expect(BootImageValidator_ValidatePackage(package, sizeof(package)) ==
             BOOT_IMAGE_ERROR_PAYLOAD_CRC,
         "image_payload_crc_reject");
}

static void TestMetadataSelection(void)
{
  OtaMetadata older = MakeMetadata(10U, OTA_STATE_STAGED);
  OtaMetadata newer = MakeMetadata(11U, OTA_STATE_STAGED);
  BootMetadataSelection selected =
      BootMetadataStore_SelectLatest(&older, &newer);

  Expect(selected.valid, "metadata_selection_valid");
  Expect(selected.metadata == &newer, "metadata_selects_newer_sequence");

  newer.record_crc32 ^= 1UL;
  selected = BootMetadataStore_SelectLatest(&older, &newer);
  Expect(selected.valid, "metadata_fallback_valid");
  Expect(selected.metadata == &older, "metadata_fallback_to_valid_copy");
}

static void TestInterruptedInstallLimit(void)
{
  OtaMetadata metadata = MakeMetadata(20U, OTA_STATE_STAGED);

  metadata.confirmed_slot = OTA_SLOT_NONE;
  metadata.install_attempts = 0U;
  Expect(BootMetadataStore_BeginInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPT_READY &&
             metadata.install_attempts == 1U,
         "install_attempt_one");
  Expect(BootMetadataStore_BeginInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPT_READY &&
             metadata.install_attempts == 2U,
         "install_resume_attempt_two");
  Expect(BootMetadataStore_BeginInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPT_READY &&
             metadata.install_attempts == 3U,
         "install_resume_attempt_three");
  Expect(BootMetadataStore_BeginInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPTS_EXHAUSTED &&
             metadata.state == OTA_STATE_FAILED,
         "install_limit_without_confirmed_fails");

  metadata.confirmed_slot = OTA_SLOT_A;
  metadata.candidate_slot = OTA_SLOT_B;
  metadata.state = OTA_STATE_INSTALLING;
  metadata.install_attempts = 3U;
  Expect(BootMetadataStore_BeginInstallAttempt(&metadata) ==
             BOOT_INSTALL_ROLLBACK_REQUIRED &&
             metadata.state == OTA_STATE_ROLLBACK_PENDING,
         "install_limit_with_confirmed_rolls_back");
}

static void TestConfirmedInstallLimit(void)
{
  OtaMetadata metadata = MakeMetadata(30U, OTA_STATE_ROLLBACK_PENDING);

  metadata.install_attempts = 0U;
  Expect(BootMetadataStore_BeginConfirmedInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPT_READY &&
             metadata.install_attempts == 1U,
         "confirmed_install_attempt_one");
  Expect(BootMetadataStore_BeginConfirmedInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPT_READY &&
             metadata.install_attempts == 2U,
         "confirmed_install_attempt_two");
  Expect(BootMetadataStore_BeginConfirmedInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPT_READY &&
             metadata.install_attempts == 3U,
         "confirmed_install_attempt_three");
  Expect(BootMetadataStore_BeginConfirmedInstallAttempt(&metadata) ==
             BOOT_INSTALL_ATTEMPTS_EXHAUSTED &&
             metadata.state == OTA_STATE_FAILED,
         "confirmed_install_limit_fails");
}

static void TestInstallPowerCutRecovery(void)
{
  OtaMetadata metadata = MakeMetadata(35U, OTA_STATE_INSTALLING);
  uint32_t attempt;

  for (attempt = 1U; attempt <= BOOT_INSTALL_ATTEMPT_LIMIT;
       ++attempt) {
    metadata.install_attempts = attempt;
    Expect(BootInstallRecovery_DecideCandidate(&metadata, true) ==
               BOOT_INSTALL_RECOVERY_SALVAGE,
           "candidate_success_before_commit_is_salvaged");
  }
  metadata.install_attempts = BOOT_INSTALL_ATTEMPT_LIMIT;
  Expect(BootInstallRecovery_DecideCandidate(&metadata, false) ==
             BOOT_INSTALL_RECOVERY_ROLLBACK,
         "candidate_invalid_after_limit_rolls_back");
  metadata.confirmed_slot = OTA_SLOT_NONE;
  Expect(BootInstallRecovery_DecideCandidate(&metadata, false) ==
             BOOT_INSTALL_RECOVERY_FAILED,
         "candidate_invalid_without_confirmed_fails");

  metadata = MakeMetadata(36U, OTA_STATE_ROLLBACK_PENDING);
  for (attempt = 1U; attempt <= BOOT_INSTALL_ATTEMPT_LIMIT;
       ++attempt) {
    metadata.install_attempts = attempt;
    Expect(BootInstallRecovery_DecideConfirmed(&metadata, true) ==
               BOOT_INSTALL_RECOVERY_SALVAGE,
           "confirmed_success_before_commit_is_salvaged");
  }
  metadata.install_attempts = BOOT_INSTALL_ATTEMPT_LIMIT;
  Expect(BootInstallRecovery_DecideConfirmed(&metadata, false) ==
             BOOT_INSTALL_RECOVERY_FAILED,
         "confirmed_invalid_after_limit_fails");

  metadata.install_attempts = BOOT_INSTALL_ATTEMPT_LIMIT - 1U;
  Expect(BootInstallRecovery_DecideCandidate(&metadata, false) ==
             BOOT_INSTALL_RECOVERY_BEGIN,
         "candidate_below_limit_reinstalls");
  Expect(BootInstallRecovery_DecideConfirmed(&metadata, false) ==
             BOOT_INSTALL_RECOVERY_BEGIN,
         "confirmed_below_limit_reinstalls");

  metadata.state = OTA_STATE_STAGED;
  metadata.install_attempts = 0U;
  Expect(BootInstallRecovery_DecideCandidate(&metadata, true) ==
             BOOT_INSTALL_RECOVERY_BEGIN,
         "staged_candidate_does_not_salvage");
  metadata.state = OTA_STATE_CONFIRMED;
  metadata.install_attempts = 1U;
  Expect(BootInstallRecovery_DecideConfirmed(&metadata, true) ==
             BOOT_INSTALL_RECOVERY_BEGIN,
         "confirmed_state_does_not_salvage");

  metadata.state = OTA_STATE_INSTALLING;
  metadata.install_attempts = BOOT_INSTALL_ATTEMPT_LIMIT;
  metadata.trial_boot_count = 2U;
  metadata.last_error = BOOT_INSTALL_ERROR_VERIFY;
  BootInstallRecovery_MarkTrial(&metadata);
  Expect(metadata.state == OTA_STATE_TRIAL &&
             metadata.install_attempts == 0U &&
             metadata.trial_boot_count == 0U &&
             metadata.last_error == 0U,
         "candidate_salvage_commits_trial_state");

  metadata.state = OTA_STATE_ROLLBACK_PENDING;
  metadata.candidate_slot = OTA_SLOT_B;
  metadata.install_attempts = BOOT_INSTALL_ATTEMPT_LIMIT;
  metadata.trial_boot_count = 3U;
  metadata.last_error = BOOT_INSTALL_ERROR_PROGRAM;
  BootInstallRecovery_MarkConfirmed(&metadata, 2048U, 0x89ABCDEFUL);
  Expect(metadata.state == OTA_STATE_CONFIRMED &&
             metadata.candidate_slot == OTA_SLOT_NONE &&
             metadata.image_size == 2048U &&
             metadata.image_crc32 == 0x89ABCDEFUL &&
             metadata.install_attempts == 0U &&
             metadata.trial_boot_count == 0U &&
             metadata.last_error == 0U,
         "confirmed_salvage_restores_metadata_semantics");
}

static void TestMetadataStateSlots(void)
{
  OtaMetadata metadata = MakeMetadata(40U, OTA_STATE_EMPTY);

  metadata.confirmed_slot = OTA_SLOT_NONE;
  metadata.candidate_slot = OTA_SLOT_NONE;
  metadata.record_crc32 = BootCrc32_Calculate(
      &metadata, sizeof(metadata) - sizeof(metadata.record_crc32));
  Expect(BootMetadataStore_Validate(&metadata) == BOOT_METADATA_OK,
         "empty_requires_no_slots_valid");
  metadata.candidate_slot = OTA_SLOT_A;
  metadata.record_crc32 = BootCrc32_Calculate(
      &metadata, sizeof(metadata) - sizeof(metadata.record_crc32));
  Expect(BootMetadataStore_Validate(&metadata) ==
             BOOT_METADATA_ERROR_SLOT,
         "empty_rejects_candidate");

  metadata.state = OTA_STATE_RECEIVING;
  metadata.confirmed_slot = OTA_SLOT_NONE;
  metadata.candidate_slot = OTA_SLOT_A;
  metadata.record_crc32 = BootCrc32_Calculate(
      &metadata, sizeof(metadata) - sizeof(metadata.record_crc32));
  Expect(BootMetadataStore_Validate(&metadata) == BOOT_METADATA_OK,
         "receiving_requires_candidate_valid");
  metadata.candidate_slot = OTA_SLOT_NONE;
  metadata.record_crc32 = BootCrc32_Calculate(
      &metadata, sizeof(metadata) - sizeof(metadata.record_crc32));
  Expect(BootMetadataStore_Validate(&metadata) ==
             BOOT_METADATA_ERROR_SLOT,
         "receiving_rejects_no_candidate");
}

static void TestFactoryMetadataDetection(void)
{
  OtaMetadata metadata;

  memset(&metadata, 0xFF, sizeof(metadata));
  Expect(BootMetadataStore_IsErased(&metadata),
         "blank_metadata_is_factory");
  metadata.magic = OTA_METADATA_MAGIC;
  Expect(!BootMetadataStore_IsErased(&metadata),
         "corrupt_metadata_is_not_factory");
}

int main(void)
{
  TestCrc32();
  TestImageValidation();
  TestMetadataSelection();
  TestInterruptedInstallLimit();
  TestConfirmedInstallLimit();
  TestInstallPowerCutRecovery();
  TestMetadataStateSlots();
  TestFactoryMetadataDetection();

  if (failures != 0) {
    printf("bootloader_core_test failed: %d\n", failures);
    return 1;
  }

  printf("bootloader_core_test passed\n");
  return 0;
}
