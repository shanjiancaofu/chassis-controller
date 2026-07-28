#ifndef OTA_UART_TRANSPORT_H
#define OTA_UART_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../../shared/ota_protocol.h"

void OtaUartTransport_Init(void);
void OtaUartTransport_Enable(void);
void OtaUartTransport_Disable(void);
void OtaUartTransport_Run(void);
bool OtaUartTransport_IsEnabled(void);
bool OtaUartTransport_TakeMessage(OtaMessage *message);
bool OtaUartTransport_SendResponse(const OtaResponse *response);
bool OtaUartTransport_IsTxIdle(void);
uint32_t OtaUartTransport_GetErrorCount(void);

#endif
