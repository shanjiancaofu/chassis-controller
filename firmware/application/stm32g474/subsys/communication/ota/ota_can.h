#ifndef OTA_CAN_H
#define OTA_CAN_H

#include <stdbool.h>

#include "device.h"
#include "drivers/can.h"
#include "../../../../../shared/ota_protocol.h"

void OtaCan_Init(const struct device *device);
bool OtaCan_OnRxFrame(const struct can_frame *frame);
bool OtaCan_TakeMessage(OtaMessage *message);
bool OtaCan_SendResponse(const OtaResponse *response);
void OtaCan_ResponseAccepted(void);
bool OtaCan_IsTxIdle(void);
void OtaCan_Invalidate(void);
uint32_t OtaCan_GetDroppedCount(void);

#endif
