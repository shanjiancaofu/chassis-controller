#include "app/system_status_collector.h"

#include "FreeRTOS.h"
#include "drivers/button/button.h"
#include "drivers/sensor/icm45686.h"
#include "drivers/adc/power_sample.h"
#include "drivers/rtc.h"
#include "devicetree_generated.h"
#include "drivers/sensor/sr501.h"
#include "communication/can_transport/can_transport.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "communication/ota_transport/ota_confirmation.h"
#include "communication/ota_transport/ota_session.h"
#include "communication/ota_transport/ota_uart_transport.h"
#include "subsys/telemetry/telemetry.h"
#include "modules/chassis/odometry.h"
#include "modules/chassis/wheel_controller.h"
#include "modules/diagnostics/board_health.h"
#include "modules/diagnostics/system_status.h"
#include "modules/parameters/parameter_manager.h"
#include "modules/safety/fault_manager.h"
#include "modules/safety/safety_manager.h"
#include "modules/sensors/imu_orientation.h"
#include "rtos/rtos_app.h"
#include "task.h"
#include "tests/target/motor_target_test.h"
#include "tests/target/qspi_target_test.h"
#include "ui/lcd/lcd_ui.h"

_Static_assert((uint32_t)BOARD_BUTTON_COUNT ==
                   (uint32_t)SYSTEM_STATUS_BUTTON_COUNT,
               "button snapshot count mismatch");

static SystemStatusCanState MapCanState(CanTransportLinkStatus state)
{
  switch (state) {
    case CAN_TRANSPORT_LINK_PASSED:
      return SYSTEM_STATUS_CAN_PASSED;
    case CAN_TRANSPORT_LINK_FAILED:
      return SYSTEM_STATUS_CAN_FAILED;
    default:
      return SYSTEM_STATUS_CAN_READY;
  }
}

static SystemStatusLcdState MapLcdState(LcdUiStatus state)
{
  switch (state) {
    case LCD_UI_DRAWING:
      return SYSTEM_STATUS_LCD_DRAWING;
    case LCD_UI_READY:
      return SYSTEM_STATUS_LCD_READY;
    case LCD_UI_FAILED:
      return SYSTEM_STATUS_LCD_FAILED;
    default:
      return SYSTEM_STATUS_LCD_DISABLED;
  }
}

static SystemStatusImuState MapImuState(Icm45686Stm32Status state)
{
  switch (state) {
    case ICM45686_NOT_FOUND:
      return SYSTEM_STATUS_IMU_NOT_FOUND;
    case ICM45686_READY:
      return SYSTEM_STATUS_IMU_READY;
    case ICM45686_DEGRADED:
      return SYSTEM_STATUS_IMU_DEGRADED;
    default:
      return SYSTEM_STATUS_IMU_UNINITIALIZED;
  }
}

static SystemStatusTaskState MapTaskState(RtosAppTaskState state)
{
  switch (state) {
    case RTOS_APP_TASK_RUNNING:
      return SYSTEM_STATUS_TASK_RUNNING;
    case RTOS_APP_TASK_TIMEOUT:
      return SYSTEM_STATUS_TASK_TIMEOUT;
    default:
      return SYSTEM_STATUS_TASK_NOT_STARTED;
  }
}

static void CopyRuntimeSnapshot(SystemRuntimeSnapshot *destination,
                                const RtosAppRuntimeSnapshot *source)
{
  destination->uptime_ms = source->uptime_ms;
  destination->service_heartbeat_age_ms = source->service_heartbeat_age_ms;
  destination->control_heartbeat_age_ms = source->control_heartbeat_age_ms;
  destination->diagnostics_heartbeat_age_ms =
      source->diagnostics_heartbeat_age_ms;
  destination->display_heartbeat_age_ms = source->display_heartbeat_age_ms;
  destination->service_period_ms = source->service_period_ms;
  destination->control_period_ms = source->control_period_ms;
  destination->diagnostics_period_ms = source->diagnostics_period_ms;
  destination->display_period_ms = source->display_period_ms;
  destination->service_expected_period_ms =
      source->service_expected_period_ms;
  destination->control_expected_period_ms =
      source->control_expected_period_ms;
  destination->diagnostics_expected_period_ms =
      source->diagnostics_expected_period_ms;
  destination->display_expected_period_ms =
      source->display_expected_period_ms;
  destination->service_timeout_ms = source->service_timeout_ms;
  destination->control_timeout_ms = source->control_timeout_ms;
  destination->diagnostics_timeout_ms = source->diagnostics_timeout_ms;
  destination->display_timeout_ms = source->display_timeout_ms;
  destination->service_run_count = source->service_run_count;
  destination->control_run_count = source->control_run_count;
  destination->diagnostics_run_count = source->diagnostics_run_count;
  destination->display_run_count = source->display_run_count;
  destination->service_stack_high_water_words =
      source->service_stack_high_water_words;
  destination->control_stack_high_water_words =
      source->control_stack_high_water_words;
  destination->diagnostics_stack_high_water_words =
      source->diagnostics_stack_high_water_words;
  destination->display_stack_high_water_words =
      source->display_stack_high_water_words;
  destination->service_task_state = MapTaskState(source->service_task_state);
  destination->control_task_state = MapTaskState(source->control_task_state);
  destination->diagnostics_task_state =
      MapTaskState(source->diagnostics_task_state);
  destination->display_task_state = MapTaskState(source->display_task_state);
  destination->service_task_healthy = source->service_task_healthy;
  destination->control_task_healthy = source->control_task_healthy;
  destination->diagnostics_task_healthy = source->diagnostics_task_healthy;
  destination->display_task_healthy = source->display_task_healthy;
  destination->critical_tasks_healthy = source->critical_tasks_healthy;
}

static void CopyButtonSnapshot(SystemStatusButtonSnapshot *destination,
                               const ButtonSnapshot *source)
{
  for (uint32_t index = 0U; index < SYSTEM_STATUS_BUTTON_COUNT; ++index) {
    destination->pressed[index] = source->pressed[index];
    destination->pressed_count[index] = source->pressed_count[index];
  }
}

static void CopyImuSnapshot(SystemStatusImuSnapshot *destination,
                            const Icm45686Stm32Snapshot *source,
                            const ImuOrientationSnapshot *orientation)
{
  destination->status = MapImuState(source->status);
  destination->who_am_i = source->who_am_i;
  destination->sample_valid = source->sample_valid;
  destination->orientation_valid = orientation->orientation_valid;
  destination->kalman_valid = orientation->kalman_valid;
  destination->fifo_timestamp = source->fifo_timestamp;
  destination->roll_rad = orientation->roll_rad;
  destination->pitch_rad = orientation->pitch_rad;
  destination->yaw_rad = orientation->yaw_rad;
  destination->kalman_roll_rad = orientation->kalman_roll_rad;
  destination->kalman_pitch_rad = orientation->kalman_pitch_rad;
  destination->last_sample_ms = source->last_sample_ms;
  destination->sample_count = source->sample_count;
  destination->fifo_frame_count = source->fifo_frame_count;
  destination->fifo_parse_error_count = source->fifo_parse_error_count;
  destination->timestamp_error_count = source->timestamp_error_count;
}

static void CopySr501Snapshot(SystemStatusSr501Snapshot *destination,
                              const Sr501Snapshot *source)
{
  destination->status = source->status == SR501_READY
                            ? SYSTEM_STATUS_SR501_READY
                            : SYSTEM_STATUS_SR501_WARMING_UP;
  destination->raw_high = source->raw_high;
  destination->motion_detected = source->motion_detected;
  destination->event_count = source->event_count;
  destination->last_motion_ms = source->last_motion_ms;
  destination->warmup_remaining_ms = source->warmup_remaining_ms;
}

static void CopyPowerSnapshot(SystemStatusPowerSampleSnapshot *destination,
                              const PowerSampleSnapshot *source)
{
  destination->valid = source->valid;
  destination->millivolts = source->millivolts;
  destination->sample_timestamp_ms = source->sample_timestamp_ms;
  destination->sample_age_ms = source->sample_age_ms;
}

void SystemStatusCollector_Update(uint32_t now_ms)
{
  SystemStatusSnapshot status = {0};
  RtosAppRuntimeSnapshot runtime;
  ButtonSnapshot buttons;
  Icm45686Stm32Snapshot imu;
  ImuOrientationSnapshot orientation;
  Sr501Snapshot sr501;
  PowerSampleSnapshot power_sample;
  MotorTargetTestSnapshot motor_test;
  RtcDateTime date_time = {0};

  BoardHealth_GetSnapshot(&status.board_health);
  RtosApp_GetRuntimeSnapshot(&runtime);
  CopyRuntimeSnapshot(&status.runtime, &runtime);

  Button_GetSnapshot(&buttons);
  sensor_get_snapshot(DEVICE_DT_GET(DT_NODE_IMU0), &imu);
  ImuOrientation_GetSnapshot(&orientation);
  Sr501_GetSnapshot(&sr501);
  power_sample_get_snapshot(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_POWER), now_ms, &power_sample);
  CopyButtonSnapshot(&status.buttons, &buttons);
  CopyImuSnapshot(&status.imu, &imu, &orientation);
  CopySr501Snapshot(&status.sr501, &sr501);
  CopyPowerSnapshot(&status.power_sample, &power_sample);

  status.can_state = MapCanState(CanTransport_GetLinkStatus());
  status.lcd_state = MapLcdState(LcdUi_GetStatus());
  status.supply_valid = power_sample_read_millivolts(DEVICE_DT_GET(DT_CHOSEN_CHASSIS_POWER), &status.supply_mv);
  status.fault_flags = FaultManager_GetFlags();
  status.qspi_test_state = (uint32_t)QspiTargetTest_GetStatus();
  status.ota_confirmation_state = (uint32_t)OtaConfirmation_GetStatus();
  status.ota_source = (uint32_t)OtaSession_GetSource();
  status.ota_state = (uint32_t)OtaSession_GetState();
  status.ota_next_offset = OtaSession_GetNextOffset();
  status.uart_error_count = OtaUartTransport_GetErrorCount();
  status.can_drop_count = OtaCanTransport_GetDroppedCount();
  status.telemetry_mode = (uint32_t)Telemetry_GetMode();

  taskENTER_CRITICAL();
  WheelController_GetSnapshot(&status.wheels);
  Odometry_GetSnapshot(now_ms, &status.odometry);
  MotorTargetTest_GetSnapshot(&motor_test);
  status.motor_test.running = motor_test.running;
  status.motor_test.left_duty = motor_test.left_duty;
  status.motor_test.right_duty = motor_test.right_duty;
  ParameterManager_GetActive(&status.parameters);
  status.control_state = SafetyManager_GetState();
  taskEXIT_CRITICAL();

  status.rtc_valid = rtc_read_datetime(&date_time);
  status.rtc_year = date_time.year;
  status.rtc_month = date_time.month;
  status.rtc_date = date_time.date;
  status.rtc_hours = date_time.hours;
  status.rtc_minutes = date_time.minutes;
  status.rtc_seconds = date_time.seconds;
  SystemStatus_Update(&status);
}
