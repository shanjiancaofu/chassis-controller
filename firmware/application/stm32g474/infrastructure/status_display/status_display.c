#include "infrastructure/status_display/status_display.h"

#include "bsp/lcd/bsp_lcd.h"
#include "infrastructure/console/diagnostic_report.h"
#include "main.h"
#include "modules/diagnostics/system_status.h"

#define KEY_DEBOUNCE_MS 20U
#define KEY_LOCKOUT_MS 100U
#define STATUS_DISPLAY_REFRESH_MS 1000U

static volatile bool key_edge_pending;
static volatile uint32_t key_edge_ms;
static uint32_t last_key_press_ms;
static uint32_t last_refresh_ms;

bool StatusDisplay_Init(void)
{
  return BspLcd_Init();
}

void StatusDisplay_Run(uint32_t now_ms)
{
  if (key_edge_pending && now_ms - key_edge_ms >= KEY_DEBOUNCE_MS) {
    key_edge_pending = false;
    if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET &&
        now_ms - last_key_press_ms >= KEY_LOCKOUT_MS) {
      last_key_press_ms = now_ms;
      BoardHealth_NotifyButtonPressed();
      DiagnosticReport_RequestSelfTest();
    }
  }

  if (last_refresh_ms == 0U ||
      now_ms - last_refresh_ms >= STATUS_DISPLAY_REFRESH_MS) {
    SystemStatusSnapshot system_status;
    BspLcdStatusData lcd_data = {0};

    last_refresh_ms = now_ms;
    SystemStatus_GetSnapshot(&system_status);
    if (system_status.rtc_valid) {
      lcd_data.rtc_state = BSP_LCD_VALUE_PASS;
      lcd_data.rtc_hours = system_status.rtc_hours;
      lcd_data.rtc_minutes = system_status.rtc_minutes;
      lcd_data.rtc_seconds = system_status.rtc_seconds;
    } else {
      lcd_data.rtc_state = BSP_LCD_VALUE_FAIL;
    }

    lcd_data.qspi_state = system_status.board_health.qspi_id_valid
                              ? BSP_LCD_VALUE_PASS
                              : BSP_LCD_VALUE_FAIL;
    lcd_data.qspi_capacity_mib =
        (uint8_t)(system_status.board_health.qspi_capacity_bytes /
                  (1024UL * 1024UL));
    lcd_data.can_state =
        system_status.can_state == SYSTEM_STATUS_CAN_PASSED
            ? BSP_LCD_VALUE_PASS
            : system_status.can_state == SYSTEM_STATUS_CAN_FAILED
                  ? BSP_LCD_VALUE_FAIL
                  : BSP_LCD_VALUE_READY;
    lcd_data.adc_valid = system_status.supply_valid;
    lcd_data.adc_mv = system_status.supply_valid ? system_status.supply_mv : 0U;
    lcd_data.fault_flags = system_status.fault_flags;
    BspLcd_SetStatusData(&lcd_data);
  }

  BspLcd_Run();
}

void StatusDisplay_OnKeyInterrupt(void)
{
  key_edge_ms = HAL_GetTick();
  key_edge_pending = true;
}
