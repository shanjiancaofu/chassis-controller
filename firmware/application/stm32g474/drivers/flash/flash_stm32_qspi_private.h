#ifndef CHASSIS_FLASH_STM32_QSPI_PRIVATE_H
#define CHASSIS_FLASH_STM32_QSPI_PRIVATE_H

#include "drivers/flash.h"
#include "stm32g4xx_hal.h"

typedef struct {
  QSPI_HandleTypeDef *handle;
} FlashStm32QspiConfig;

typedef struct {
  volatile FlashTransferStatus transfer_status;
} FlashStm32QspiData;

extern const FlashDriverApi flash_stm32_qspi_api;
int FlashStm32Qspi_Init(const struct device *device);

#endif
