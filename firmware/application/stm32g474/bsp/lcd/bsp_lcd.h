#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_LCD_WIDTH 320U
#define BSP_LCD_HEIGHT 240U

typedef enum
{
  BSP_LCD_DISABLED = 0,
  BSP_LCD_READY,
  BSP_LCD_TRANSMITTING,
  BSP_LCD_FAILED
} BspLcdStatus;

bool BspLcd_Init(void);
bool BspLcd_BeginFrame(void);
bool BspLcd_TransmitRow(const uint8_t *row_data, uint16_t size);
bool BspLcd_IsRowTransferComplete(void);
bool BspLcd_HasTransferError(void);
void BspLcd_EndFrame(void);
BspLcdStatus BspLcd_GetStatus(void);
void BspLcd_OnSpiTxComplete(void);
void BspLcd_OnSpiError(void);

#endif
