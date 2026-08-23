#include "ui/lcd/lcd_status_presenter.h"
#include "device.h"
#include "devicetree.h"

#include "drivers/button/button.h"
#include "drivers/reset/reset.h"
#include "config/app_config.h"
#include "modules/diagnostics/system_status.h"
#include "ui/lcd/lcd_ui.h"

#define KEY_LOCKOUT_MS 100U
#define STATUS_DISPLAY_REFRESH_MS 1000U

static uint32_t last_key_press_ms;
static uint32_t last_refresh_ms;

static uint8_t EstimateBatteryPercent(uint32_t supply_mv)
{
  if (supply_mv <= BATTERY_PERCENT_EMPTY_MV) {
    return 0U;
  }
  if (supply_mv >= BATTERY_PERCENT_FULL_MV) {
    return 100U;
  }
  return (uint8_t)(((supply_mv - BATTERY_PERCENT_EMPTY_MV) * 100U) /
                   (BATTERY_PERCENT_FULL_MV - BATTERY_PERCENT_EMPTY_MV));
}

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

static int32_t MilliFromFloat(float value)
{
  const float scaled = value * 1000.0f;

  if (!(scaled == scaled)) {
    return 0;
  }
  if (scaled >= 999999.0f) {
    return 999999;
  }
  if (scaled <= -999999.0f) {
    return -999999;
  }
  return (int32_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

static LcdUiControlState MapControlState(ChassisControlState state)
{
  switch (state) {
    case CHASSIS_CONTROL_RUNNING:
      return LCD_UI_CONTROL_RUNNING;
    case CHASSIS_CONTROL_COMMAND_TIMEOUT:
      return LCD_UI_CONTROL_TIMEOUT;
    case CHASSIS_CONTROL_EMERGENCY_STOP:
      return LCD_UI_CONTROL_EMERGENCY_STOP;
    case CHASSIS_CONTROL_INTERNAL_FAULT:
      return LCD_UI_CONTROL_FAULT;
    case CHASSIS_CONTROL_OPEN_LOOP_TEST:
      return LCD_UI_CONTROL_TEST;
    default:
      return LCD_UI_CONTROL_STOPPED;
  }
}

static LcdUiSensorState MapImuState(SystemStatusImuState state)
{
  switch (state) {
    case SYSTEM_STATUS_IMU_READY:
      return LCD_UI_SENSOR_READY;
    case SYSTEM_STATUS_IMU_DEGRADED:
      return LCD_UI_SENSOR_DEGRADED;
    case SYSTEM_STATUS_IMU_NOT_FOUND:
      return LCD_UI_SENSOR_FAILED;
    default:
      return LCD_UI_SENSOR_DISABLED;
  }
}

static LcdUiSensorState MapSr501State(SystemStatusSr501State state)
{
  return state == SYSTEM_STATUS_SR501_READY ? LCD_UI_SENSOR_READY
                                             : LCD_UI_SENSOR_WARMING;
}

static LcdUiResetCause MapResetCause(uint32_t flags)
{
  if ((flags & RESET_CAUSE_IWDG) != 0U) {
    return LCD_UI_RESET_IWDG;
  }
  if ((flags & RESET_CAUSE_WWDG) != 0U) {
    return LCD_UI_RESET_WWDG;
  }
  if ((flags & RESET_CAUSE_SOFTWARE) != 0U) {
    return LCD_UI_RESET_SOFTWARE;
  }
  if ((flags & RESET_CAUSE_PIN) != 0U) {
    return LCD_UI_RESET_PIN;
  }
  if ((flags & RESET_CAUSE_BOR) != 0U) {
    return LCD_UI_RESET_BROWNOUT;
  }
  if ((flags & RESET_CAUSE_LOW_POWER) != 0U) {
    return LCD_UI_RESET_LOW_POWER;
  }
  if ((flags & RESET_CAUSE_OPTION_BYTE) != 0U) {
    return LCD_UI_RESET_OPTION_BYTE;
  }
  return LCD_UI_RESET_NONE;
}

static void RefreshLcdData(void)
{
  SystemStatusSnapshot system_status;
  LcdUiStatusData lcd_data = {0};

  SystemStatus_GetSnapshot(&system_status);

  lcd_data.control_state = MapControlState(system_status.control_state);
  lcd_data.can_state = system_status.can_state == SYSTEM_STATUS_CAN_PASSED
                           ? LCD_UI_VALUE_PASS
                           : system_status.can_state == SYSTEM_STATUS_CAN_FAILED
                                 ? LCD_UI_VALUE_FAIL
                                 : LCD_UI_VALUE_READY;
  lcd_data.qspi_state = system_status.board_health.qspi_id_valid
                            ? LCD_UI_VALUE_PASS
                            : LCD_UI_VALUE_FAIL;
  lcd_data.rtc_state = system_status.rtc_valid ? LCD_UI_VALUE_PASS
                                                : LCD_UI_VALUE_FAIL;
  lcd_data.supply_state = system_status.supply_valid ? LCD_UI_VALUE_PASS
                                                      : LCD_UI_VALUE_FAIL;
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
  lcd_data.odometry_valid = system_status.odometry.valid;
  lcd_data.odometry_x_mm = MilliFromFloat(system_status.odometry.x_m);
  lcd_data.odometry_y_mm = MilliFromFloat(system_status.odometry.y_m);
  lcd_data.odometry_heading_mrad =
      MilliFromFloat(system_status.odometry.heading_rad);
  lcd_data.odometry_linear_mm_s =
      MilliFromFloat(system_status.odometry.linear_velocity_mps);

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
  lcd_data.battery_percent_valid = system_status.supply_valid;
  lcd_data.battery_percent = system_status.supply_valid
                                 ? EstimateBatteryPercent(system_status.supply_mv)
                                 : 0U;
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

  LcdUi_SetStatusData(&lcd_data);
}

static LcdUiPage NextPage(LcdUiPage page)
{
  const uint32_t next_page = (uint32_t)page + 1U;

  return next_page >= LCD_UI_PAGE_COUNT ? LCD_UI_PAGE_OVERVIEW
                                         : (LcdUiPage)next_page;
}

bool LcdStatusPresenter_Init(void)
{
  return LcdUi_Init(DEVICE_DT_GET(DT_CHOSEN(chassis_display)));
}

void LcdStatusPresenter_Run(uint32_t now_ms)
{
  if (button_take_display_key(DEVICE_DT_GET(DT_CHOSEN(chassis_buttons))) &&
      now_ms - last_key_press_ms >= KEY_LOCKOUT_MS) {
    last_key_press_ms = now_ms;
    LcdUi_SetPage(NextPage(LcdUi_GetPage()));
    last_refresh_ms = 0U;
    BoardHealth_NotifyButtonPressed();
  }

  if (last_refresh_ms == 0U ||
      now_ms - last_refresh_ms >= STATUS_DISPLAY_REFRESH_MS) {
    last_refresh_ms = now_ms;
    RefreshLcdData();
  }

  LcdUi_Run();
}
