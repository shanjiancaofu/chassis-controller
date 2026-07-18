#ifndef FDCAN_DRIVER_H
#define FDCAN_DRIVER_H

#include "stm32g4xx_hal.h"

typedef enum {
  FDCAN_LOOPBACK_PENDING = 0,
  FDCAN_LOOPBACK_PASSED,
  FDCAN_LOOPBACK_FAILED
} FdcanLoopbackStatus;

HAL_StatusTypeDef FdcanDriver_Init(void);
FdcanLoopbackStatus FdcanDriver_GetLoopbackStatus(void);

#endif
