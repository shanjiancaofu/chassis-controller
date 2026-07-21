#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdbool.h>

typedef enum
{
  BSP_LCD_DISABLED = 0,
  BSP_LCD_DRAWING,
  BSP_LCD_READY,
  BSP_LCD_FAILED
} BspLcdStatus;

bool BspLcd_Init(void);
void BspLcd_Run(void);
BspLcdStatus BspLcd_GetStatus(void);

#endif
