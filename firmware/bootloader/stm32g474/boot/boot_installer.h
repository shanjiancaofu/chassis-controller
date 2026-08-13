#ifndef BOOT_INSTALLER_H
#define BOOT_INSTALLER_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../shared/firmware_image.h"
#include "../../../shared/ota_metadata.h"

typedef enum
{
  BOOT_INSTALL_OK = 0,
  BOOT_INSTALL_ERROR_SLOT,
  BOOT_INSTALL_ERROR_READ,
  BOOT_INSTALL_ERROR_HEADER,
  BOOT_INSTALL_ERROR_METADATA,
  BOOT_INSTALL_ERROR_PAYLOAD_CRC,
  BOOT_INSTALL_ERROR_VECTOR,
  BOOT_INSTALL_ERROR_FLASH_LAYOUT,
  BOOT_INSTALL_ERROR_ERASE,
  BOOT_INSTALL_ERROR_PROGRAM,
  BOOT_INSTALL_ERROR_VERIFY
} BootInstallStatus;

typedef enum
{
  BOOT_INSTALL_FAILURE_PRE_DESTRUCTIVE = 0,
  BOOT_INSTALL_FAILURE_DESTRUCTIVE_OR_UNCERTAIN,
  BOOT_INSTALL_FAILURE_GLOBAL_FATAL
} BootInstallFailureClass;

typedef enum
{
  BOOT_RECOVERY_VERIFY_MATCH = 0,
  BOOT_RECOVERY_VERIFY_INTERNAL_MISMATCH,
  BOOT_RECOVERY_VERIFY_SOURCE_INVALID,
  BOOT_RECOVERY_VERIFY_IO_ERROR
} BootRecoveryVerifyStatus;

static inline bool BootInstaller_RecoveryAllowsReinstall(
    BootRecoveryVerifyStatus status)
{
  return status == BOOT_RECOVERY_VERIFY_INTERNAL_MISMATCH;
}

BootInstallStatus BootInstaller_Install(const OtaMetadata *metadata,
                                        OtaSlotId slot);
bool BootInstaller_ReadImageHeader(OtaSlotId slot, OtaImageHeader *header);
static inline BootInstallFailureClass
BootInstaller_ClassifyFailure(BootInstallStatus status)
{
  switch (status) {
    case BOOT_INSTALL_ERROR_SLOT:
    case BOOT_INSTALL_ERROR_READ:
    case BOOT_INSTALL_ERROR_HEADER:
    case BOOT_INSTALL_ERROR_METADATA:
    case BOOT_INSTALL_ERROR_PAYLOAD_CRC:
    case BOOT_INSTALL_ERROR_VECTOR:
      return BOOT_INSTALL_FAILURE_PRE_DESTRUCTIVE;

    case BOOT_INSTALL_ERROR_ERASE:
    case BOOT_INSTALL_ERROR_PROGRAM:
    case BOOT_INSTALL_ERROR_VERIFY:
      return BOOT_INSTALL_FAILURE_DESTRUCTIVE_OR_UNCERTAIN;

    case BOOT_INSTALL_ERROR_FLASH_LAYOUT:
    case BOOT_INSTALL_OK:
    default:
      return BOOT_INSTALL_FAILURE_GLOBAL_FATAL;
  }
}
bool BootInstaller_VerifyInstalledCandidate(const OtaMetadata *metadata);
BootRecoveryVerifyStatus BootInstaller_VerifyInstalledRecovery(
    OtaSlotId slot);
bool BootInstaller_VerifyInstalled(OtaSlotId slot);

#endif
