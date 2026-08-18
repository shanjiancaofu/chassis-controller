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
  BSP_LCD_PAGE_OVERVIEW = 0,
  BSP_LCD_PAGE_MOTOR,
  BSP_LCD_PAGE_SENSORS,
  BSP_LCD_PAGE_SYSTEM,
  BSP_LCD_PAGE_COUNT
} BspLcdPage;

typedef enum
{
  BSP_LCD_VALUE_READY = 0,
  BSP_LCD_VALUE_PASS,
  BSP_LCD_VALUE_FAIL,
  BSP_LCD_VALUE_DISABLED
} BspLcdValueState;

typedef enum
{
  BSP_LCD_CONTROL_STOPPED = 0,
  BSP_LCD_CONTROL_RUNNING,
  BSP_LCD_CONTROL_TIMEOUT,
  BSP_LCD_CONTROL_EMERGENCY_STOP,
  BSP_LCD_CONTROL_FAULT,
  BSP_LCD_CONTROL_TEST
} BspLcdControlState;

typedef enum
{
  BSP_LCD_SENSOR_DISABLED = 0,
  BSP_LCD_SENSOR_WARMING,
  BSP_LCD_SENSOR_READY,
  BSP_LCD_SENSOR_DEGRADED,
  BSP_LCD_SENSOR_FAILED
} BspLcdSensorState;

typedef enum
{
  BSP_LCD_RESET_NONE = 0,
  BSP_LCD_RESET_PIN,
  BSP_LCD_RESET_BROWNOUT,
  BSP_LCD_RESET_SOFTWARE,
  BSP_LCD_RESET_IWDG,
  BSP_LCD_RESET_WWDG,
  BSP_LCD_RESET_LOW_POWER,
  BSP_LCD_RESET_OPTION_BYTE
} BspLcdResetCause;

typedef struct
{
  BspLcdControlState control_state;
  BspLcdValueState can_state;
  BspLcdValueState qspi_state;
  BspLcdValueState rtc_state;
  BspLcdValueState supply_state;
  BspLcdSensorState imu_state;
  BspLcdSensorState sr501_state;

  int32_t left_target;
  int32_t right_target;
  int32_t left_measurement;
  int32_t right_measurement;
  int32_t left_encoder_delta;
  int32_t right_encoder_delta;
  int16_t left_output;
  int16_t right_output;

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
  BspLcdResetCause reset_cause;
  bool critical_tasks_healthy;
  bool service_task_healthy;
  bool control_task_healthy;
  bool diagnostics_task_healthy;
  bool display_task_healthy;
  uint32_t service_stack_free_words;
  uint32_t control_stack_free_words;
  uint32_t diagnostics_stack_free_words;
  uint32_t display_stack_free_words;
} BspLcdStatusData;

bool BspLcd_Init(void);
void BspLcd_Run(void);
void BspLcd_SetPage(BspLcdPage page);
void BspLcd_SetStatusData(const BspLcdStatusData *data);
BspLcdStatus BspLcd_GetStatus(void);
BspLcdPage BspLcd_GetPage(void);
void BspLcd_OnSpiTxComplete(void);
void BspLcd_OnSpiError(void);

#endif
