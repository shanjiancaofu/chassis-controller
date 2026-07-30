#ifndef OTA_SESSION_H
#define OTA_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../../shared/ota_protocol.h"

void OtaSession_Init(void);
bool OtaSession_Submit(const OtaMessage *message, uint32_t now_ms,
                       bool begin_allowed);
void OtaSession_Run(uint32_t now_ms);
bool OtaSession_TakeResponse(OtaResponse *response);
void OtaSession_ResponseSubmitted(void);
void OtaSession_AbortSource(OtaSource source, uint32_t now_ms);
OtaTransferState OtaSession_GetState(void);
OtaSource OtaSession_GetSource(void);
uint32_t OtaSession_GetNextOffset(void);
bool OtaSession_IsActive(void);
bool OtaSession_IsUsingQspi(void);
bool OtaSession_HasCriticalFault(void);
bool OtaSession_IsResetRequested(uint32_t now_ms);

#endif
