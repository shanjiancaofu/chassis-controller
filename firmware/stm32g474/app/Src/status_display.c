#include "status_display.h"

#include "board_self_test.h"
#include "bsp_lcd.h"
#include "bsp_power_sample.h"
#include "chassis_control.h"
#include "fdcan_driver.h"
#include "main.h"
#include "rtc.h"

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
      BspLcd_SetPage(BspLcd_GetPage() == BSP_LCD_PAGE_COVER
                         ? BSP_LCD_PAGE_STATUS
                         : BSP_LCD_PAGE_COVER);
      BoardSelfTest_NotifyButtonPressed();
    }
  }

  if (last_refresh_ms == 0U ||
      now_ms - last_refresh_ms >= STATUS_DISPLAY_REFRESH_MS) {
    BoardSelfTestStatus self_test_status;
    ChassisControlStatus chassis_status;
    BspLcdStatusData lcd_data = {0};
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    uint32_t supply_mv;
    const FdcanLoopbackStatus can_status =
        FdcanDriver_GetLoopbackStatus();

    last_refresh_ms = now_ms;
    BoardSelfTest_GetStatus(&self_test_status);
    ChassisControl_GetStatus(&chassis_status);

    if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK &&
        HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK &&
        time.Hours <= 23U && time.Minutes <= 59U &&
        time.Seconds <= 59U) {
      lcd_data.rtc_state = BSP_LCD_VALUE_PASS;
      lcd_data.rtc_hours = time.Hours;
      lcd_data.rtc_minutes = time.Minutes;
      lcd_data.rtc_seconds = time.Seconds;
    } else {
      lcd_data.rtc_state = BSP_LCD_VALUE_FAIL;
    }

    lcd_data.qspi_state = self_test_status.qspi_id_valid
                              ? BSP_LCD_VALUE_PASS
                              : BSP_LCD_VALUE_FAIL;
    lcd_data.qspi_capacity_mib =
        (uint8_t)(self_test_status.qspi_capacity_bytes /
                  (1024UL * 1024UL));
    lcd_data.can_state =
        can_status == FDCAN_LOOPBACK_PASSED
            ? BSP_LCD_VALUE_PASS
            : can_status == FDCAN_LOOPBACK_FAILED
                  ? BSP_LCD_VALUE_FAIL
                  : BSP_LCD_VALUE_READY;
    lcd_data.adc_valid = BspPowerSample_ReadMillivolts(&supply_mv);
    lcd_data.adc_mv = lcd_data.adc_valid ? supply_mv : 0U;
    lcd_data.fault_flags = chassis_status.fault_flags;
    BspLcd_SetStatusData(&lcd_data);
  }

  BspLcd_Run();
}

void StatusDisplay_OnKeyInterrupt(void)
{
  key_edge_ms = HAL_GetTick();
  key_edge_pending = true;
}
