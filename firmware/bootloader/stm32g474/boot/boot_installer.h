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

BootInstallStatus BootInstaller_Install(const OtaMetadata *metadata,
                                        OtaSlotId slot);
bool BootInstaller_ReadImageHeader(OtaSlotId slot, OtaImageHeader *header);
bool BootInstaller_VerifyInstalledCandidate(const OtaMetadata *metadata);
bool BootInstaller_VerifyInstalled(OtaSlotId slot);

#endif
