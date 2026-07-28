#ifndef OTA_CONFIRMATION_H
#define OTA_CONFIRMATION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  OTA_CONFIRMATION_WAITING = 0,
  OTA_CONFIRMATION_NOT_REQUIRED,
  OTA_CONFIRMATION_RUNNING,
  OTA_CONFIRMATION_CONFIRMED,
  OTA_CONFIRMATION_FAILED
} OtaConfirmationStatus;

void OtaConfirmation_Init(void);
void OtaConfirmation_Run(uint32_t now_ms, bool startup_healthy,
                         bool qspi_available);
OtaConfirmationStatus OtaConfirmation_GetStatus(void);
bool OtaConfirmation_IsUsingQspi(void);

#endif
