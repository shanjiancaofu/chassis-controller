#ifndef OTA_CAN_TRANSPORT_H
#define OTA_CAN_TRANSPORT_H

#include <stdbool.h>

#include "bsp/fdcan/fdcan_bsp.h"
#include "../../../../shared/ota_protocol.h"

void OtaCanTransport_Init(void);
bool OtaCanTransport_OnRxFrameFromIsr(const BspFdcanFrame *frame);
bool OtaCanTransport_TakeMessage(OtaMessage *message);
bool OtaCanTransport_SendResponse(const OtaResponse *response);
bool OtaCanTransport_IsTxIdle(void);
void OtaCanTransport_Invalidate(void);
uint32_t OtaCanTransport_GetDroppedCount(void);

#endif
