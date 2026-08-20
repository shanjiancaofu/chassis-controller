#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_WIDTH 320U
#define LCD_HEIGHT 240U

typedef enum
{
  LCD_DISABLED = 0,
  LCD_READY,
  LCD_TRANSMITTING,
  LCD_FAILED
} LcdStatus;

bool Lcd_Init(void);
bool Lcd_BeginFrame(void);
bool Lcd_TransmitRow(const uint8_t *row_data, uint16_t size);
bool Lcd_IsRowTransferComplete(void);
bool Lcd_HasTransferError(void);
void Lcd_EndFrame(void);
LcdStatus Lcd_GetStatus(void);
void Lcd_OnSpiTxComplete(void);
void Lcd_OnSpiError(void);

#endif
