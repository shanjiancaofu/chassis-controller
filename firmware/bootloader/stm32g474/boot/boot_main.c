#include "boot/boot_main.h"

#include <stdbool.h>
#include <stdint.h>

#include "boot/app_launcher.h"
#include "boot/boot_install_recovery.h"
#include "boot/boot_installer.h"
#include "boot/boot_metadata_io.h"
#include "boot/boot_trace.h"
#include "bsp/flash/boot_internal_flash.h"
#include "bsp/qspi/boot_qspi_flash.h"
#include "config/build_info.h"
#include "iwdg.h"
#include "../../../shared/qspi_flash_identity.h"

#define BOOT_TRIAL_LIMIT 3U

static void BeginCandidateInstall(BootMetadataSnapshot *snapshot);
static void BeginConfirmedInstall(BootMetadataSnapshot *snapshot);
static void InstallCandidate(BootMetadataSnapshot *snapshot);
static void InstallConfirmed(BootMetadataSnapshot *snapshot);
static void CommitTrialAndReset(BootMetadataSnapshot *snapshot);
static void CommitConfirmedAndReset(BootMetadataSnapshot *snapshot);
static void HandleCandidateFailure(BootMetadataSnapshot *snapshot,
                                   BootInstallStatus status);
static void HandleInvalidTrial(BootMetadataSnapshot *snapshot);
static bool CommitAndReload(BootMetadataSnapshot *snapshot,
                            OtaMetadata *next);
static void JumpFactoryApplicationIfVectorValid(void)
    __attribute__((noreturn));
static void JumpVerifiedConfirmed(const OtaMetadata *metadata)
    __attribute__((noreturn));
static void EnterRecovery(void) __attribute__((noreturn));

void BootMain_Run(void)
{
  uint8_t jedec_id[3];
  BootMetadataSnapshot snapshot;
  OtaMetadata next;

  BootTrace_Init();
  BootWatchdog_Refresh();
  BootTrace_Write("BOOT: VERSION=" BOOTLOADER_VERSION_STRING
                  " BUILD=" BOOTLOADER_BUILD_STRING "\r\n");
  BootTrace_Write("BOOT: QSPI CHECK\r\n");
  if (!BootQspiFlash_ReadJedecId(jedec_id)) {
    BootTrace_Write("BOOT: QSPI ID READ FAILED\r\n");
    EnterRecovery();
  }
  BootWatchdog_Refresh();
  BootTrace_Write("BOOT: QSPI READY\r\n");
  if (!QspiFlashIdentity_IsSupported(jedec_id)) {
    BootTrace_Write("BOOT: QSPI ID UNSUPPORTED\r\n");
    EnterRecovery();
  }
  BootTrace_Write("BOOT: METADATA CHECK\r\n");
  if (!BootMetadataIo_Load(&snapshot)) {
    BootTrace_Write("BOOT: METADATA READ FAILED\r\n");
    EnterRecovery();
  }
  BootWatchdog_Refresh();
  BootTrace_Write("BOOT: METADATA READY\r\n");

  if (!snapshot.selection.valid) {
    BootTrace_Write("BOOT: NO METADATA\r\n");
    if (BootMetadataStore_IsErased(&snapshot.copy_a) &&
        BootMetadataStore_IsErased(&snapshot.copy_b)) {
      JumpFactoryApplicationIfVectorValid();
    }
    BootTrace_Write("BOOT: METADATA CORRUPT\r\n");
    EnterRecovery();
  }
  BootTrace_WriteValue("BOOT: STATE=",
                       snapshot.selection.metadata->state);

  switch ((OtaState)snapshot.selection.metadata->state) {
    case OTA_STATE_STAGED:
      BeginCandidateInstall(&snapshot);
      break;

    case OTA_STATE_INSTALLING:
      BeginCandidateInstall(&snapshot);
      break;

    case OTA_STATE_TRIAL:
      next = *snapshot.selection.metadata;
      ++next.trial_boot_count;
      if (next.trial_boot_count > BOOT_TRIAL_LIMIT) {
        if (next.confirmed_slot == OTA_SLOT_NONE) {
          next.state = OTA_STATE_FAILED;
          next.last_error = BOOT_INSTALL_ERROR_SLOT;
          (void)CommitAndReload(&snapshot, &next);
          EnterRecovery();
        }
        next.state = OTA_STATE_ROLLBACK_PENDING;
        next.install_attempts = 0U;
      }
      if (!CommitAndReload(&snapshot, &next)) {
        BootTrace_Write("BOOT: TRIAL COUNT COMMIT FAILED\r\n");
        EnterRecovery();
      }
      if (next.state == OTA_STATE_ROLLBACK_PENDING) {
        BeginConfirmedInstall(&snapshot);
      }
      if (!BootInstaller_VerifyInstalled(
              (OtaSlotId)next.candidate_slot)) {
        BootTrace_Write("BOOT: TRIAL VERIFY FAILED\r\n");
        HandleInvalidTrial(&snapshot);
      }
      BootTrace_Write("BOOT: TRIAL VERIFIED\r\n");
      BootAppLauncher_Jump();
      break;

    case OTA_STATE_ROLLBACK_PENDING:
      BeginConfirmedInstall(&snapshot);
      break;

    case OTA_STATE_CONFIRMED:
      if (!BootInstaller_VerifyInstalled(
              (OtaSlotId)snapshot.selection.metadata->confirmed_slot)) {
        BeginConfirmedInstall(&snapshot);
      }
      BootAppLauncher_Jump();
      break;

    case OTA_STATE_EMPTY:
    case OTA_STATE_RECEIVING:
      JumpVerifiedConfirmed(snapshot.selection.metadata);
      break;

    case OTA_STATE_FAILED:
    default:
      EnterRecovery();
      break;
  }

  EnterRecovery();
}

static void BeginCandidateInstall(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
  BootInstallRecoveryAction recovery = BOOT_INSTALL_RECOVERY_BEGIN;
  BootInstallAttemptDecision decision;

  if (next.install_attempts >= BOOT_INSTALL_ATTEMPT_LIMIT) {
    recovery = BootInstallRecovery_DecideCandidate(
        &next, BootInstaller_VerifyInstalledCandidate(&next));
  }
  if (recovery == BOOT_INSTALL_RECOVERY_SALVAGE) {
    BootTrace_Write("BOOT: SALVAGE INSTALLED CANDIDATE\r\n");
    CommitTrialAndReset(snapshot);
  }
  if (recovery == BOOT_INSTALL_RECOVERY_ROLLBACK) {
    BootTrace_Write("BOOT: CANDIDATE LIMIT, ROLLBACK\r\n");
    next.state = OTA_STATE_ROLLBACK_PENDING;
    next.install_attempts = 0U;
    next.last_error = BOOT_INSTALL_ERROR_VERIFY;
    if (CommitAndReload(snapshot, &next)) {
      BeginConfirmedInstall(snapshot);
    }
    EnterRecovery();
  }
  if (recovery == BOOT_INSTALL_RECOVERY_FAILED) {
    BootTrace_Write("BOOT: CANDIDATE LIMIT, RECOVERY\r\n");
    next.state = OTA_STATE_FAILED;
    next.last_error = BOOT_INSTALL_ERROR_VERIFY;
    (void)CommitAndReload(snapshot, &next);
    EnterRecovery();
  }

  decision = BootMetadataStore_BeginInstallAttempt(&next);

  if (decision != BOOT_INSTALL_ATTEMPT_READY) {
    BootTrace_Write("BOOT: INSTALL LIMIT REACHED\r\n");
    next.last_error = BOOT_INSTALL_ERROR_VERIFY;
    if (decision == BOOT_INSTALL_ROLLBACK_REQUIRED) {
      next.install_attempts = 0U;
      if (CommitAndReload(snapshot, &next)) {
        BeginConfirmedInstall(snapshot);
      }
    } else {
      (void)CommitAndReload(snapshot, &next);
    }
    EnterRecovery();
  }

  if (!CommitAndReload(snapshot, &next)) {
    BootTrace_Write("BOOT: INSTALLING COMMIT FAILED\r\n");
    EnterRecovery();
  }
  InstallCandidate(snapshot);
}

static void BeginConfirmedInstall(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
  BootInstallRecoveryAction recovery = BOOT_INSTALL_RECOVERY_BEGIN;

  if (next.install_attempts >= BOOT_INSTALL_ATTEMPT_LIMIT) {
    recovery = BootInstallRecovery_DecideConfirmed(
        &next, BootInstaller_VerifyInstalled(
                   (OtaSlotId)next.confirmed_slot));
  }
  if (recovery == BOOT_INSTALL_RECOVERY_SALVAGE) {
    BootTrace_Write("BOOT: SALVAGE INSTALLED CONFIRMED\r\n");
    CommitConfirmedAndReset(snapshot);
  }
  if (recovery == BOOT_INSTALL_RECOVERY_FAILED) {
    BootTrace_Write("BOOT: CONFIRMED LIMIT, RECOVERY\r\n");
    next.state = OTA_STATE_FAILED;
    next.last_error = BOOT_INSTALL_ERROR_VERIFY;
    (void)CommitAndReload(snapshot, &next);
    EnterRecovery();
  }

  if (BootMetadataStore_BeginConfirmedInstallAttempt(&next) !=
      BOOT_INSTALL_ATTEMPT_READY) {
    BootTrace_Write("BOOT: CONFIRMED INSTALL LIMIT REACHED\r\n");
    next.last_error = BOOT_INSTALL_ERROR_VERIFY;
    (void)CommitAndReload(snapshot, &next);
    EnterRecovery();
  }
  if (!CommitAndReload(snapshot, &next)) {
    BootTrace_Write("BOOT: ROLLBACK ATTEMPT COMMIT FAILED\r\n");
    EnterRecovery();
  }
  InstallConfirmed(snapshot);
}

static void InstallCandidate(BootMetadataSnapshot *snapshot)
{
  BootInstallStatus status;

  BootTrace_Write("BOOT: INSTALL CANDIDATE\r\n");
  status = BootInstaller_Install(
      snapshot->selection.metadata,
      (OtaSlotId)snapshot->selection.metadata->candidate_slot);

  if (status != BOOT_INSTALL_OK) {
    BootTrace_WriteValue("BOOT: INSTALL FAILED=", status);
    if (status == BOOT_INSTALL_ERROR_ERASE) {
      BootTrace_WriteValue("BOOT: ERASE BANK=",
                           BootInternalFlash_GetLastEraseBank());
      BootTrace_WriteValue("BOOT: ERASE PAGE=",
                           BootInternalFlash_GetLastErasePage());
      BootTrace_WriteValue("BOOT: FLASH ERROR=",
                           BootInternalFlash_GetLastError());
    }
    HandleCandidateFailure(snapshot, status);
  }
  BootTrace_Write("BOOT: INSTALL VERIFIED\r\n");
  CommitTrialAndReset(snapshot);
}

static void HandleCandidateFailure(BootMetadataSnapshot *snapshot,
                                   BootInstallStatus status)
{
  OtaMetadata next = *snapshot->selection.metadata;

  next.last_error = (uint32_t)status;
  if (next.confirmed_slot != OTA_SLOT_NONE) {
    next.state = OTA_STATE_ROLLBACK_PENDING;
    next.install_attempts = 0U;
    if (CommitAndReload(snapshot, &next)) {
      BeginConfirmedInstall(snapshot);
    }
  } else {
    next.state = OTA_STATE_FAILED;
    (void)CommitAndReload(snapshot, &next);
  }
  EnterRecovery();
}

static void HandleInvalidTrial(BootMetadataSnapshot *snapshot)
{
  HandleCandidateFailure(snapshot, BOOT_INSTALL_ERROR_VERIFY);
}

static void InstallConfirmed(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
  BootInstallStatus status;

  status = BootInstaller_Install(
      snapshot->selection.metadata,
      (OtaSlotId)snapshot->selection.metadata->confirmed_slot);

  if (status != BOOT_INSTALL_OK) {
    BootTrace_WriteValue("BOOT: ROLLBACK FAILED=", status);
    next.state = OTA_STATE_ROLLBACK_PENDING;
    next.last_error = (uint32_t)status;
    if (!CommitAndReload(snapshot, &next)) {
      EnterRecovery();
    }
    BootWatchdog_Refresh();
    NVIC_SystemReset();
    EnterRecovery();
  }

  CommitConfirmedAndReset(snapshot);
}

static void CommitTrialAndReset(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;

  BootInstallRecovery_MarkTrial(&next);
  if (!CommitAndReload(snapshot, &next)) {
    BootTrace_Write("BOOT: TRIAL COMMIT FAILED\r\n");
    EnterRecovery();
  }
  BootTrace_Write("BOOT: TRIAL COMMITTED\r\n");
  BootWatchdog_Refresh();
  NVIC_SystemReset();
  EnterRecovery();
}

static void CommitConfirmedAndReset(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
  OtaImageHeader header;

  if (!BootInstaller_ReadImageHeader(
          (OtaSlotId)next.confirmed_slot, &header)) {
    BootTrace_Write("BOOT: CONFIRMED HEADER READ FAILED\r\n");
    EnterRecovery();
  }
  BootInstallRecovery_MarkConfirmed(
      &next, header.payload_size, header.payload_crc32);
  if (!CommitAndReload(snapshot, &next)) {
    BootTrace_Write("BOOT: CONFIRMED COMMIT FAILED\r\n");
    EnterRecovery();
  }
  BootWatchdog_Refresh();
  NVIC_SystemReset();
  EnterRecovery();
}

static bool CommitAndReload(BootMetadataSnapshot *snapshot,
                            OtaMetadata *next)
{
  return BootMetadataIo_Commit(snapshot, next) &&
         BootMetadataIo_Load(snapshot) && snapshot->selection.valid;
}

static void JumpFactoryApplicationIfVectorValid(void)
{
  if (BootAppLauncher_IsApplicationValid()) {
    BootTrace_Write("BOOT: JUMP APPLICATION\r\n");
    BootAppLauncher_Jump();
  }
  BootTrace_Write("BOOT: APPLICATION INVALID\r\n");
  EnterRecovery();
}

static void JumpVerifiedConfirmed(const OtaMetadata *metadata)
{
  if (metadata->confirmed_slot == OTA_SLOT_NONE) {
    BootTrace_Write("BOOT: NO CONFIRMED IMAGE\r\n");
    EnterRecovery();
  }
  if (BootInstaller_VerifyInstalled(
          (OtaSlotId)metadata->confirmed_slot)) {
    BootAppLauncher_Jump();
  }
  BootTrace_Write("BOOT: CONFIRMED IMAGE INVALID\r\n");
  EnterRecovery();
}

static void EnterRecovery(void)
{
  BootTrace_Write("BOOT: RECOVERY\r\n");
  for (;;) {
    __WFI();
  }
}
