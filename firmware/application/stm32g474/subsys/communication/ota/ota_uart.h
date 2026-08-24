#ifndef OTA_UART_H
#define OTA_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../../../shared/ota_protocol.h"

void OtaUart_Init(void);
void OtaUart_Enable(void);
void OtaUart_Disable(void);
void OtaUart_Run(void);
bool OtaUart_IsEnabled(void);
bool OtaUart_TakeMessage(OtaMessage *message);
bool OtaUart_SendResponse(const OtaResponse *response);
bool OtaUart_IsTxIdle(void);
uint32_t OtaUart_GetErrorCount(void);

#endif
