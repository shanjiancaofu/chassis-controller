#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "app/modules/chassis/odometry.h"
#include "app/modules/chassis/wheel_controller.h"
#include "app/modules/diagnostics/board_health.h"
#include "app/modules/parameters/parameter_manager.h"
#include "app/modules/safety/safety_manager.h"

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

typedef enum {
  SYSTEM_STATUS_TASK_NOT_STARTED = 0,
  SYSTEM_STATUS_TASK_RUNNING,
  SYSTEM_STATUS_TASK_TIMEOUT
} SystemStatusTaskState;

typedef enum {
  SYSTEM_STATUS_IMU_UNINITIALIZED = 0,
  SYSTEM_STATUS_IMU_NOT_FOUND,
  SYSTEM_STATUS_IMU_READY,
  SYSTEM_STATUS_IMU_DEGRADED
} SystemStatusImuState;

typedef enum {
  SYSTEM_STATUS_SR501_WARMING_UP = 0,
  SYSTEM_STATUS_SR501_READY
} SystemStatusSr501State;

typedef enum {
  SYSTEM_STATUS_BUTTON_1 = 0,
  SYSTEM_STATUS_BUTTON_2,
  SYSTEM_STATUS_BUTTON_COUNT
} SystemStatusButtonId;

typedef struct {
  bool pressed[SYSTEM_STATUS_BUTTON_COUNT];
  uint32_t pressed_count[SYSTEM_STATUS_BUTTON_COUNT];
} SystemStatusButtonSnapshot;

typedef struct {
  SystemStatusImuState status;
  uint8_t who_am_i;
  bool sample_valid;
  bool orientation_valid;
  bool kalman_valid;
  uint16_t fifo_timestamp;
  float roll_rad;
  float pitch_rad;
  float yaw_rad;
  float kalman_roll_rad;
  float kalman_pitch_rad;
  uint32_t last_sample_ms;
  uint32_t sample_count;
  uint32_t fifo_frame_count;
  uint32_t fifo_parse_error_count;
  uint32_t timestamp_error_count;
} SystemStatusImuSnapshot;

typedef struct {
  SystemStatusSr501State status;
  bool raw_high;
  bool motion_detected;
  uint32_t event_count;
  uint32_t last_motion_ms;
  uint32_t warmup_remaining_ms;
} SystemStatusSr501Snapshot;

typedef struct {
  bool valid;
  uint32_t millivolts;
  uint32_t sample_timestamp_ms;
  uint32_t sample_age_ms;
} SystemStatusPowerSampleSnapshot;

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
  SystemStatusTaskState service_task_state;
  SystemStatusTaskState control_task_state;
  SystemStatusTaskState diagnostics_task_state;
  SystemStatusTaskState display_task_state;
  bool service_task_healthy;
  bool control_task_healthy;
  bool diagnostics_task_healthy;
  bool display_task_healthy;
  bool critical_tasks_healthy;
} SystemRuntimeSnapshot;

typedef struct {
  BoardHealthSnapshot board_health;
  SystemRuntimeSnapshot runtime;
  SystemStatusButtonSnapshot buttons;
  SystemStatusImuSnapshot imu;
  SystemStatusSr501Snapshot sr501;
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
  SystemStatusPowerSampleSnapshot power_sample;
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
