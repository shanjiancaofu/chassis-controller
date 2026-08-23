#ifndef LCD_STM32_PRIVATE_H
#define LCD_STM32_PRIVATE_H

#include "drivers/display/lcd.h"

typedef struct {
  volatile bool dma_complete;
  volatile bool dma_failed;
  volatile bool dma_pending;
  LcdStatus status;
} DisplayStm32Data;

extern const DisplayDriverApi display_stm32_api;
int DisplayStm32_Init(const struct device *device);

#endif
