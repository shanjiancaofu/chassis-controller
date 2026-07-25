#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  BSP_LCD_DISABLED = 0,
  BSP_LCD_DRAWING,
  BSP_LCD_READY,
  BSP_LCD_FAILED
} BspLcdStatus;

typedef enum
{
  BSP_LCD_PAGE_COVER = 0,
  BSP_LCD_PAGE_STATUS
} BspLcdPage;

typedef enum
{
  BSP_LCD_VALUE_READY = 0,
  BSP_LCD_VALUE_PASS,
  BSP_LCD_VALUE_FAIL,
  BSP_LCD_VALUE_DISABLED
} BspLcdValueState;

typedef struct
{
  BspLcdValueState rtc_state;
  uint8_t rtc_hours;
  uint8_t rtc_minutes;
  uint8_t rtc_seconds;
  BspLcdValueState qspi_state;
  uint8_t qspi_capacity_mib;
  BspLcdValueState can_state;
  bool adc_valid;
  uint32_t adc_mv;
  uint32_t fault_flags;
} BspLcdStatusData;

bool BspLcd_Init(void);
void BspLcd_Run(void);
void BspLcd_SetPage(BspLcdPage page);
void BspLcd_SetStatusData(const BspLcdStatusData *data);
BspLcdStatus BspLcd_GetStatus(void);
BspLcdPage BspLcd_GetPage(void);

#endif
