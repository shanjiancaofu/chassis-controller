#include "infrastructure/status_display/status_display.h"

#include "bsp/lcd/bsp_lcd.h"
#include "bsp/reset/bsp_reset.h"
#include "main.h"
#include "modules/diagnostics/system_status.h"

#define KEY_DEBOUNCE_MS 20U
#define KEY_LOCKOUT_MS 100U
#define STATUS_DISPLAY_REFRESH_MS 1000U

static volatile bool key_edge_pending;
static volatile uint32_t key_edge_ms;
static uint32_t last_key_press_ms;
static uint32_t last_refresh_ms;

static int16_t DegreesFromRadians(float radians)
{
  const float degrees = radians * 57.2957795F;

  if (degrees > 32767.0F) {
    return 32767;
  }
  if (degrees < -32768.0F) {
    return -32768;
  }
  return (int16_t)(degrees >= 0.0F ? degrees + 0.5F : degrees - 0.5F);
}

static BspLcdControlState MapControlState(ChassisControlState state)
{
  switch (state) {
    case CHASSIS_CONTROL_RUNNING:
      return BSP_LCD_CONTROL_RUNNING;
    case CHASSIS_CONTROL_COMMAND_TIMEOUT:
      return BSP_LCD_CONTROL_TIMEOUT;
    case CHASSIS_CONTROL_EMERGENCY_STOP:
      return BSP_LCD_CONTROL_EMERGENCY_STOP;
    case CHASSIS_CONTROL_INTERNAL_FAULT:
      return BSP_LCD_CONTROL_FAULT;
    case CHASSIS_CONTROL_OPEN_LOOP_TEST:
      return BSP_LCD_CONTROL_TEST;
    default:
      return BSP_LCD_CONTROL_STOPPED;
  }
}

static BspLcdSensorState MapImuState(BspIcm45686Status state)
{
  switch (state) {
    case BSP_ICM45686_READY:
      return BSP_LCD_SENSOR_READY;
    case BSP_ICM45686_DEGRADED:
      return BSP_LCD_SENSOR_DEGRADED;
    case BSP_ICM45686_NOT_FOUND:
      return BSP_LCD_SENSOR_FAILED;
    default:
      return BSP_LCD_SENSOR_DISABLED;
  }
}

static BspLcdSensorState MapSr501State(BspSr501Status state)
{
  return state == BSP_SR501_READY ? BSP_LCD_SENSOR_READY
                                  : BSP_LCD_SENSOR_WARMING;
}

static BspLcdResetCause MapResetCause(uint32_t flags)
{
  if ((flags & BSP_RESET_CAUSE_IWDG) != 0U) {
    return BSP_LCD_RESET_IWDG;
  }
  if ((flags & BSP_RESET_CAUSE_WWDG) != 0U) {
    return BSP_LCD_RESET_WWDG;
  }
  if ((flags & BSP_RESET_CAUSE_SOFTWARE) != 0U) {
    return BSP_LCD_RESET_SOFTWARE;
  }
  if ((flags & BSP_RESET_CAUSE_PIN) != 0U) {
    return BSP_LCD_RESET_PIN;
  }
  if ((flags & BSP_RESET_CAUSE_BOR) != 0U) {
    return BSP_LCD_RESET_BROWNOUT;
  }
  if ((flags & BSP_RESET_CAUSE_LOW_POWER) != 0U) {
    return BSP_LCD_RESET_LOW_POWER;
  }
  if ((flags & BSP_RESET_CAUSE_OPTION_BYTE) != 0U) {
    return BSP_LCD_RESET_OPTION_BYTE;
  }
  return BSP_LCD_RESET_NONE;
}

static void RefreshLcdData(void)
{
  SystemStatusSnapshot system_status;
  BspLcdStatusData lcd_data = {0};

  SystemStatus_GetSnapshot(&system_status);

  lcd_data.control_state = MapControlState(system_status.control_state);
  lcd_data.can_state = system_status.can_state == SYSTEM_STATUS_CAN_PASSED
                           ? BSP_LCD_VALUE_PASS
                           : system_status.can_state == SYSTEM_STATUS_CAN_FAILED
                                 ? BSP_LCD_VALUE_FAIL
                                 : BSP_LCD_VALUE_READY;
  lcd_data.qspi_state = system_status.board_health.qspi_id_valid
                            ? BSP_LCD_VALUE_PASS
                            : BSP_LCD_VALUE_FAIL;
  lcd_data.rtc_state = system_status.rtc_valid ? BSP_LCD_VALUE_PASS
                                                : BSP_LCD_VALUE_FAIL;
  lcd_data.supply_state = system_status.supply_valid ? BSP_LCD_VALUE_PASS
                                                      : BSP_LCD_VALUE_FAIL;
  lcd_data.imu_state = MapImuState(system_status.imu.status);
  lcd_data.sr501_state = MapSr501State(system_status.sr501.status);

  lcd_data.left_target = system_status.wheels.left_target;
  lcd_data.right_target = system_status.wheels.right_target;
  lcd_data.left_measurement = system_status.wheels.left_measurement;
  lcd_data.right_measurement = system_status.wheels.right_measurement;
  lcd_data.left_encoder_delta = system_status.odometry.left_delta;
  lcd_data.right_encoder_delta = system_status.odometry.right_delta;
  lcd_data.left_output = system_status.wheels.left_output;
  lcd_data.right_output = system_status.wheels.right_output;

  lcd_data.imu_orientation_valid = system_status.imu.orientation_valid;
  lcd_data.roll_deg = DegreesFromRadians(system_status.imu.roll_rad);
  lcd_data.pitch_deg = DegreesFromRadians(system_status.imu.pitch_rad);
  lcd_data.yaw_deg = DegreesFromRadians(system_status.imu.yaw_rad);

  lcd_data.sr501_raw_high = system_status.sr501.raw_high;
  lcd_data.sr501_motion = system_status.sr501.motion_detected;
  lcd_data.sr501_event_count = system_status.sr501.event_count;
  lcd_data.sr501_warmup_remaining_ms = system_status.sr501.warmup_remaining_ms;

  lcd_data.rtc_hours = system_status.rtc_hours;
  lcd_data.rtc_minutes = system_status.rtc_minutes;
  lcd_data.rtc_seconds = system_status.rtc_seconds;
  lcd_data.qspi_capacity_mib =
      (uint8_t)(system_status.board_health.qspi_capacity_bytes /
                (1024UL * 1024UL));
  lcd_data.adc_mv = system_status.supply_valid ? system_status.supply_mv : 0U;
  lcd_data.fault_flags = system_status.fault_flags;
  lcd_data.uptime_ms = system_status.runtime.uptime_ms;
  lcd_data.reset_cause =
      MapResetCause(system_status.board_health.reset_cause_flags);
  lcd_data.critical_tasks_healthy = system_status.runtime.critical_tasks_healthy;
  lcd_data.service_task_healthy = system_status.runtime.service_task_healthy;
  lcd_data.control_task_healthy = system_status.runtime.control_task_healthy;
  lcd_data.diagnostics_task_healthy =
      system_status.runtime.diagnostics_task_healthy;
  lcd_data.display_task_healthy = system_status.runtime.display_task_healthy;
  lcd_data.service_stack_free_words =
      system_status.runtime.service_stack_high_water_words;
  lcd_data.control_stack_free_words =
      system_status.runtime.control_stack_high_water_words;
  lcd_data.diagnostics_stack_free_words =
      system_status.runtime.diagnostics_stack_high_water_words;
  lcd_data.display_stack_free_words =
      system_status.runtime.display_stack_high_water_words;

  BspLcd_SetStatusData(&lcd_data);
}

static BspLcdPage NextPage(BspLcdPage page)
{
  const uint32_t next_page = (uint32_t)page + 1U;

  return next_page >= BSP_LCD_PAGE_COUNT ? BSP_LCD_PAGE_OVERVIEW
                                         : (BspLcdPage)next_page;
}

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
      BspLcd_SetPage(NextPage(BspLcd_GetPage()));
      last_refresh_ms = 0U;
      BoardHealth_NotifyButtonPressed();
    }
  }

  if (last_refresh_ms == 0U ||
      now_ms - last_refresh_ms >= STATUS_DISPLAY_REFRESH_MS) {
    last_refresh_ms = now_ms;
    RefreshLcdData();
  }

  BspLcd_Run();
}

void StatusDisplay_OnKeyInterrupt(void)
{
  key_edge_ms = HAL_GetTick();
  key_edge_pending = true;
}
