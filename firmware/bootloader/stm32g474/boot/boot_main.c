#include "boot/boot_main.h"

#include <stdbool.h>
#include <stdint.h>

#include "boot/app_launcher.h"
#include "boot/boot_installer.h"
#include "boot/boot_metadata_io.h"
#include "boot/boot_trace.h"
#include "bsp/flash/boot_internal_flash.h"
#include "bsp/qspi/boot_qspi_flash.h"
#include "iwdg.h"
#include "../../../shared/qspi_flash_identity.h"

#define BOOT_TRIAL_LIMIT 3U

static void InstallCandidate(BootMetadataSnapshot *snapshot);
static void InstallConfirmed(BootMetadataSnapshot *snapshot);
static void HandleCandidateFailure(BootMetadataSnapshot *snapshot,
                                   BootInstallStatus status);
static void HandleInvalidTrial(BootMetadataSnapshot *snapshot);
static bool CommitAndReload(BootMetadataSnapshot *snapshot,
                            OtaMetadata *next);
static void JumpOrWait(void) __attribute__((noreturn));
static void EnterRecovery(void) __attribute__((noreturn));

void BootMain_Run(void)
{
  uint8_t jedec_id[3];
  BootMetadataSnapshot snapshot;
  OtaMetadata next;

  BootTrace_Init();
  BootWatchdog_Refresh();
  BootTrace_Write("BOOT: QSPI CHECK\r\n");
  if (!BootQspiFlash_ReadJedecId(jedec_id)) {
    BootTrace_Write("BOOT: QSPI ID READ FAILED\r\n");
    JumpOrWait();
  }
  BootWatchdog_Refresh();
  BootTrace_Write("BOOT: QSPI READY\r\n");
  if (!QspiFlashIdentity_IsSupported(jedec_id)) {
    BootTrace_Write("BOOT: QSPI ID UNSUPPORTED\r\n");
    JumpOrWait();
  }
  BootTrace_Write("BOOT: METADATA CHECK\r\n");
  if (!BootMetadataIo_Load(&snapshot)) {
    BootTrace_Write("BOOT: METADATA READ FAILED\r\n");
    JumpOrWait();
  }
  BootWatchdog_Refresh();
  BootTrace_Write("BOOT: METADATA READY\r\n");

  if (!snapshot.selection.valid) {
    BootTrace_Write("BOOT: NO METADATA\r\n");
    JumpOrWait();
  }
  BootTrace_WriteValue("BOOT: STATE=",
                       snapshot.selection.metadata->state);

  switch ((OtaState)snapshot.selection.metadata->state) {
    case OTA_STATE_STAGED:
      next = *snapshot.selection.metadata;
      next.state = OTA_STATE_INSTALLING;
      ++next.install_attempts;
      if (!CommitAndReload(&snapshot, &next)) {
        BootTrace_Write("BOOT: INSTALLING COMMIT FAILED\r\n");
        EnterRecovery();
      }
      InstallCandidate(&snapshot);
      break;

    case OTA_STATE_INSTALLING:
      InstallCandidate(&snapshot);
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
      }
      if (!CommitAndReload(&snapshot, &next)) {
        BootTrace_Write("BOOT: TRIAL COUNT COMMIT FAILED\r\n");
        EnterRecovery();
      }
      if (next.state == OTA_STATE_ROLLBACK_PENDING) {
        InstallConfirmed(&snapshot);
      }
      if (!BootInstaller_VerifyInstalled(
              (OtaSlotId)next.candidate_slot)) {
        BootTrace_Write("BOOT: TRIAL VERIFY FAILED\r\n");
        HandleInvalidTrial(&snapshot);
      }
      BootTrace_Write("BOOT: TRIAL VERIFIED\r\n");
      JumpOrWait();
      break;

    case OTA_STATE_ROLLBACK_PENDING:
      InstallConfirmed(&snapshot);
      break;

    case OTA_STATE_CONFIRMED:
      if (!BootInstaller_VerifyInstalled(
              (OtaSlotId)snapshot.selection.metadata->confirmed_slot)) {
        InstallConfirmed(&snapshot);
      }
      BootAppLauncher_Jump();
      break;

    case OTA_STATE_EMPTY:
    case OTA_STATE_RECEIVING:
    case OTA_STATE_FAILED:
    default:
      JumpOrWait();
      break;
  }

  EnterRecovery();
}

static void InstallCandidate(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
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

  next.state = OTA_STATE_TRIAL;
  next.trial_boot_count = 0U;
  next.last_error = 0U;
  if (!CommitAndReload(snapshot, &next)) {
    BootTrace_Write("BOOT: TRIAL COMMIT FAILED\r\n");
    EnterRecovery();
  }
  BootTrace_Write("BOOT: TRIAL COMMITTED\r\n");
  BootWatchdog_Refresh();
  NVIC_SystemReset();
  EnterRecovery();
}

static void HandleCandidateFailure(BootMetadataSnapshot *snapshot,
                                   BootInstallStatus status)
{
  OtaMetadata next = *snapshot->selection.metadata;

  next.last_error = (uint32_t)status;
  if (next.confirmed_slot != OTA_SLOT_NONE) {
    next.state = OTA_STATE_ROLLBACK_PENDING;
    if (CommitAndReload(snapshot, &next)) {
      InstallConfirmed(snapshot);
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
    next.state = OTA_STATE_FAILED;
    next.last_error = (uint32_t)status;
    (void)CommitAndReload(snapshot, &next);
    EnterRecovery();
  }

  next.state = OTA_STATE_CONFIRMED;
  next.candidate_slot = OTA_SLOT_NONE;
  next.trial_boot_count = 0U;
  next.last_error = 0U;
  if (!CommitAndReload(snapshot, &next)) {
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

static void JumpOrWait(void)
{
  if (BootAppLauncher_IsApplicationValid()) {
    BootTrace_Write("BOOT: JUMP APPLICATION\r\n");
    BootAppLauncher_Jump();
  }
  BootTrace_Write("BOOT: APPLICATION INVALID\r\n");
  EnterRecovery();
}

static void EnterRecovery(void)
{
  BootTrace_Write("BOOT: RECOVERY\r\n");
  for (;;) {
    __WFI();
  }
}
