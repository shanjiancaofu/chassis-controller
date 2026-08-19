#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/button/bsp_button.h"
#include "bsp/imu/bsp_icm45686.h"
#include "bsp/power_monitor/bsp_power_sample.h"
#include "bsp/sr501/bsp_sr501.h"
#include "modules/chassis/odometry.h"
#include "modules/chassis/wheel_controller.h"
#include "modules/diagnostics/board_health.h"
#include "modules/parameters/parameter_manager.h"
#include "modules/safety/safety_manager.h"

typedef enum {
  SYSTEM_STATUS_CAN_READY = 0,
  SYSTEM_STATUS_CAN_PASSED,
  SYSTEM_STATUS_CAN_FAILED
} SystemStatusCanState;

typedef enum {
  SYSTEM_STATUS_LCD_DISABLED = 0,
  SYSTEM_STATUS_LCD_DRAWING,
  SYSTEM_STATUS_LCD_READY,
  SYSTEM_STATUS_LCD_FAILED
} SystemStatusLcdState;

typedef struct {
  bool running;
  int16_t left_duty;
  int16_t right_duty;
} SystemMotorTestSnapshot;

typedef struct {
  uint32_t uptime_ms;
  uint32_t service_heartbeat_age_ms;
  uint32_t control_heartbeat_age_ms;
  uint32_t diagnostics_heartbeat_age_ms;
  uint32_t display_heartbeat_age_ms;
  uint32_t service_period_ms;
  uint32_t control_period_ms;
  uint32_t diagnostics_period_ms;
  uint32_t display_period_ms;
  uint32_t service_expected_period_ms;
  uint32_t control_expected_period_ms;
  uint32_t diagnostics_expected_period_ms;
  uint32_t display_expected_period_ms;
  uint32_t service_timeout_ms;
  uint32_t control_timeout_ms;
  uint32_t diagnostics_timeout_ms;
  uint32_t display_timeout_ms;
  uint32_t service_run_count;
  uint32_t control_run_count;
  uint32_t diagnostics_run_count;
  uint32_t display_run_count;
  uint32_t service_stack_high_water_words;
  uint32_t control_stack_high_water_words;
  uint32_t diagnostics_stack_high_water_words;
  uint32_t display_stack_high_water_words;
  uint32_t service_task_state;
  uint32_t control_task_state;
  uint32_t diagnostics_task_state;
  uint32_t display_task_state;
  bool service_task_healthy;
  bool control_task_healthy;
  bool diagnostics_task_healthy;
  bool display_task_healthy;
  bool critical_tasks_healthy;
} SystemRuntimeSnapshot;

typedef struct {
  BoardHealthSnapshot board_health;
  SystemRuntimeSnapshot runtime;
  BspButtonSnapshot buttons;
  BspIcm45686Snapshot imu;
  BspSr501Snapshot sr501;
  WheelControllerSnapshot wheels;
  OdometrySnapshot odometry;
  SystemMotorTestSnapshot motor_test;
  ParameterSnapshot parameters;
  ChassisControlState control_state;
  SystemStatusCanState can_state;
  SystemStatusLcdState lcd_state;
  bool rtc_valid;
  uint8_t rtc_year;
  uint8_t rtc_month;
  uint8_t rtc_date;
  uint8_t rtc_hours;
  uint8_t rtc_minutes;
  uint8_t rtc_seconds;
  bool supply_valid;
  uint32_t supply_mv;
  BspPowerSampleSnapshot power_sample;
  uint32_t fault_flags;
  uint32_t qspi_test_state;
  uint32_t ota_confirmation_state;
  uint32_t ota_source;
  uint32_t ota_state;
  uint32_t ota_next_offset;
  uint32_t uart_error_count;
  uint32_t can_drop_count;
  uint32_t telemetry_mode;
} SystemStatusSnapshot;

void SystemStatus_Init(void);
void SystemStatus_Update(const SystemStatusSnapshot *snapshot);
void SystemStatus_GetSnapshot(SystemStatusSnapshot *snapshot);

#endif
