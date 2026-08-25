#ifndef LCD_UI_H
#define LCD_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

typedef enum
{
  LCD_UI_DISABLED = 0,
  LCD_UI_DRAWING,
  LCD_UI_READY,
  LCD_UI_FAILED
} LcdUiStatus;

typedef enum
{
  LCD_UI_PAGE_OVERVIEW = 0,
  LCD_UI_PAGE_MOTOR,
  LCD_UI_PAGE_SENSORS,
  LCD_UI_PAGE_SYSTEM,
  LCD_UI_PAGE_COUNT
} LcdUiPage;

typedef enum
{
  LCD_UI_VALUE_READY = 0,
  LCD_UI_VALUE_PASS,
  LCD_UI_VALUE_FAIL,
  LCD_UI_VALUE_DISABLED
} LcdUiValueState;

typedef enum
{
  LCD_UI_CONTROL_STOPPED = 0,
  LCD_UI_CONTROL_RUNNING,
  LCD_UI_CONTROL_TIMEOUT,
  LCD_UI_CONTROL_EMERGENCY_STOP,
  LCD_UI_CONTROL_FAULT,
  LCD_UI_CONTROL_TEST,
  LCD_UI_CONTROL_STARTING
} LcdUiControlState;

typedef enum
{
  LCD_UI_SENSOR_DISABLED = 0,
  LCD_UI_SENSOR_WARMING,
  LCD_UI_SENSOR_READY,
  LCD_UI_SENSOR_DEGRADED,
  LCD_UI_SENSOR_FAILED
} LcdUiSensorState;

typedef enum
{
  LCD_UI_RESET_NONE = 0,
  LCD_UI_RESET_PIN,
  LCD_UI_RESET_BROWNOUT,
  LCD_UI_RESET_SOFTWARE,
  LCD_UI_RESET_IWDG,
  LCD_UI_RESET_WWDG,
  LCD_UI_RESET_LOW_POWER,
  LCD_UI_RESET_OPTION_BYTE
} LcdUiResetCause;

typedef struct
{
  LcdUiControlState control_state;
  LcdUiValueState can_state;
  LcdUiValueState qspi_state;
  LcdUiValueState rtc_state;
  LcdUiValueState supply_state;
  LcdUiSensorState imu_state;
  LcdUiSensorState sr501_state;
  int32_t left_target;
  int32_t right_target;
  int32_t left_measurement;
  int32_t right_measurement;
  int32_t left_encoder_delta;
  int32_t right_encoder_delta;
  int16_t left_output;
  int16_t right_output;
  bool odometry_valid;
  int32_t odometry_x_mm;
  int32_t odometry_y_mm;
  int32_t odometry_heading_mrad;
  int32_t odometry_linear_mm_s;
  int16_t roll_deg;
  int16_t pitch_deg;
  int16_t yaw_deg;
  bool imu_orientation_valid;
  bool sr501_raw_high;
  bool sr501_motion;
  uint32_t sr501_event_count;
  uint32_t sr501_warmup_remaining_ms;
  uint8_t rtc_hours;
  uint8_t rtc_minutes;
  uint8_t rtc_seconds;
  uint8_t qspi_capacity_mib;
  uint32_t adc_mv;
  uint8_t battery_percent;
  bool battery_percent_valid;
  uint32_t fault_flags;
  uint32_t uptime_ms;
  LcdUiResetCause reset_cause;
  bool critical_tasks_healthy;
  bool service_task_healthy;
  bool control_task_healthy;
  bool diagnostics_task_healthy;
  bool display_task_healthy;
  uint32_t service_stack_free_words;
  uint32_t control_stack_free_words;
  uint32_t diagnostics_stack_free_words;
  uint32_t display_stack_free_words;
} LcdUiStatusData;

bool LcdUi_Init(const struct device *display);
void LcdUi_Run(void);
void LcdUi_SetPage(LcdUiPage page);
void LcdUi_SetStatusData(const LcdUiStatusData *data);
LcdUiStatus LcdUi_GetStatus(void);
LcdUiPage LcdUi_GetPage(void);

#if defined(LCD_UI_HOST_PREVIEW)
void LcdUi_PreviewPrepare(LcdUiPage page, const LcdUiStatusData *data);
void LcdUi_PreviewRenderRow(uint16_t row, uint8_t *row_data,
                           uint16_t row_size);
#endif

#endif
