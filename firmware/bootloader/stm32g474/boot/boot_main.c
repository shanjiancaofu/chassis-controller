#include "boot/boot_main.h"

#include <stdbool.h>
#include <stdint.h>

#include "boot/app_launcher.h"
#include "boot/boot_installer.h"
#include "boot/boot_metadata_io.h"
#include "bsp/qspi/boot_qspi_flash.h"
#include "iwdg.h"

#define BOOT_TRIAL_LIMIT 3U
#define W25Q64_MANUFACTURER_ID 0xEFU
#define W25Q64_MEMORY_TYPE 0x40U
#define W25Q64_CAPACITY_ID 0x17U

static void InstallCandidate(BootMetadataSnapshot *snapshot);
static void InstallConfirmed(BootMetadataSnapshot *snapshot);
static bool CommitAndReload(BootMetadataSnapshot *snapshot,
                            OtaMetadata *next);
static void JumpOrWait(void) __attribute__((noreturn));
static void WaitForRecovery(void) __attribute__((noreturn));

void BootMain_Run(void)
{
  uint8_t jedec_id[3];
  BootMetadataSnapshot snapshot;
  OtaMetadata next;

  if (!BootQspiFlash_ReadJedecId(jedec_id) ||
      jedec_id[0] != W25Q64_MANUFACTURER_ID ||
      jedec_id[1] != W25Q64_MEMORY_TYPE ||
      jedec_id[2] != W25Q64_CAPACITY_ID ||
      !BootMetadataIo_Load(&snapshot)) {
    JumpOrWait();
  }

  if (!snapshot.selection.valid) {
    JumpOrWait();
  }

  switch ((OtaState)snapshot.selection.metadata->state) {
    case OTA_STATE_STAGED:
      next = *snapshot.selection.metadata;
      next.state = OTA_STATE_INSTALLING;
      ++next.install_attempts;
      if (!CommitAndReload(&snapshot, &next)) {
        WaitForRecovery();
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
          WaitForRecovery();
        }
        next.state = OTA_STATE_ROLLBACK_PENDING;
      }
      if (!CommitAndReload(&snapshot, &next)) {
        WaitForRecovery();
      }
      if (next.state == OTA_STATE_ROLLBACK_PENDING) {
        InstallConfirmed(&snapshot);
      }
      JumpOrWait();
      break;

    case OTA_STATE_ROLLBACK_PENDING:
      InstallConfirmed(&snapshot);
      break;

    case OTA_STATE_CONFIRMED:
      if (!BootAppLauncher_IsApplicationValid()) {
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

  WaitForRecovery();
}

static void InstallCandidate(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
  BootInstallStatus status;

  BootWatchdog_Start();
  status = BootInstaller_Install(
      snapshot->selection.metadata,
      (OtaSlotId)snapshot->selection.metadata->candidate_slot);

  if (status != BOOT_INSTALL_OK) {
    next.state = OTA_STATE_FAILED;
    next.last_error = (uint32_t)status;
    (void)CommitAndReload(snapshot, &next);
    WaitForRecovery();
  }

  next.state = OTA_STATE_TRIAL;
  next.trial_boot_count = 0U;
  next.last_error = 0U;
  if (!CommitAndReload(snapshot, &next)) {
    WaitForRecovery();
  }
  NVIC_SystemReset();
  WaitForRecovery();
}

static void InstallConfirmed(BootMetadataSnapshot *snapshot)
{
  OtaMetadata next = *snapshot->selection.metadata;
  BootInstallStatus status;

  BootWatchdog_Start();
  status = BootInstaller_Install(
      snapshot->selection.metadata,
      (OtaSlotId)snapshot->selection.metadata->confirmed_slot);

  if (status != BOOT_INSTALL_OK) {
    next.state = OTA_STATE_FAILED;
    next.last_error = (uint32_t)status;
    (void)CommitAndReload(snapshot, &next);
    WaitForRecovery();
  }

  next.state = OTA_STATE_CONFIRMED;
  next.candidate_slot = OTA_SLOT_NONE;
  next.trial_boot_count = 0U;
  next.last_error = 0U;
  if (!CommitAndReload(snapshot, &next)) {
    WaitForRecovery();
  }
  NVIC_SystemReset();
  WaitForRecovery();
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
    BootAppLauncher_Jump();
  }
  WaitForRecovery();
}

static void WaitForRecovery(void)
{
  for (;;) {
    BootWatchdog_Refresh();
  }
}
