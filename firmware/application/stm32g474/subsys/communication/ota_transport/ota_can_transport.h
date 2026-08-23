#ifndef OTA_CAN_TRANSPORT_H
#define OTA_CAN_TRANSPORT_H

#include <stdbool.h>

#include "device.h"
#include "drivers/can.h"
#include "../../../../../shared/ota_protocol.h"

void OtaCanTransport_Init(const struct device *device);
bool OtaCanTransport_OnRxFrame(const struct can_frame *frame);
bool OtaCanTransport_TakeMessage(OtaMessage *message);
bool OtaCanTransport_SendResponse(const OtaResponse *response);
void OtaCanTransport_ResponseAccepted(void);
bool OtaCanTransport_IsTxIdle(void);
void OtaCanTransport_Invalidate(void);
uint32_t OtaCanTransport_GetDroppedCount(void);

#endif
