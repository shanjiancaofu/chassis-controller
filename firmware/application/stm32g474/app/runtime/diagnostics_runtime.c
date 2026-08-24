#include "app/runtime/diagnostics_runtime.h"

#include <stdio.h>

#include "app/diagnostics/system_status.h"
#include "app/maintenance/self_test/iwdg_self_test.h"
#include "app/safety/fault_manager.h"
#include "app/sensors/imu_orientation.h"
#include "app/diagnostics/system_status_collector.h"
#include "config/app_config.h"
#include "devicetree.h"
#include "drivers/led/led.h"
#include "drivers/sensor/icm45686.h"
#include "drivers/sensor/sr501.h"
#include "drivers/time.h"
#include "drivers/watchdog.h"
#include "subsys/communication/can_transport/can_transport.h"
#include "subsys/communication/uart_protocol/uart_protocol.h"

static const struct device *imu;
static uint32_t last_heartbeat_ms;

static void ProcessImuSample(const Icm45686Stm32Sample *sample)
{
  if (sample != NULL) {
    ImuOrientation_ProcessSample(sample->accel_mps2, sample->gyro_rad_s,
                                 sample->sample_period_s);
  }
}

static const Icm45686Stm32SampleSink imu_sample_sink = {
    .reset = ImuOrientation_Reset,
    .process_sample = ProcessImuSample,
};

bool DiagnosticsRuntime_Init(const struct device *imu_device)
{
  last_heartbeat_ms = 0U;
  imu = imu_device;
#if CONFIG_ICM45686
  {
    Icm45686Stm32Snapshot snapshot;
    char fields[48];

    ImuOrientation_Init();
    if (!device_is_ready(imu) ||
        !sensor_set_sample_sink(imu, &imu_sample_sink)) {
      return false;
    }
    sensor_get_snapshot(imu, &snapshot);
    (void)snprintf(fields, sizeof(fields),
                   "device=ICM45686 who_am_i=0x%02X",
                   (unsigned int)snapshot.who_am_i);
    if (snapshot.status == ICM45686_READY) {
      (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_INFO,
                                 "imu", "READY", fields);
    } else if (snapshot.status == ICM45686_NOT_FOUND) {
      (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_WARN,
                                 "imu", "NOT_FOUND", fields);
    } else {
      (void)UartProtocol_SendLog(time_uptime_ms(), UART_PROTOCOL_LOG_ERROR,
                                 "imu", "INIT_FAILED", fields);
    }
  }
#else
  (void)imu;
#endif
  return true;
}

void DiagnosticsRuntime_Run(void)
{
  const uint32_t now_ms = time_uptime_ms();

  sr501_run(DEVICE_DT_GET(DT_CHOSEN(chassis_sr501)), now_ms);
#if CONFIG_ICM45686
  sensor_run(imu, now_ms);
#endif
  SystemStatusCollector_Update(now_ms);

  if (now_ms - last_heartbeat_ms >= 500U) {
    last_heartbeat_ms = now_ms;
    led_toggle(DEVICE_DT_GET(DT_CHOSEN(chassis_leds)), LED_BLUE);
  }
  led_set(DEVICE_DT_GET(DT_CHOSEN(chassis_leds)), LED_GREEN, false);
  led_set(DEVICE_DT_GET(DT_CHOSEN(chassis_leds)), LED_RED, false);
  if (CanTransport_GetLinkStatus() == CAN_TRANSPORT_LINK_PASSED) {
    led_set(DEVICE_DT_GET(DT_CHOSEN(chassis_leds)), LED_GREEN, true);
  } else if (CanTransport_GetLinkStatus() == CAN_TRANSPORT_LINK_FAILED) {
    led_set(DEVICE_DT_GET(DT_CHOSEN(chassis_leds)), LED_RED, true);
  }

  {
    SystemStatusSnapshot status;

    SystemStatus_GetSnapshot(&status);
    if (status.runtime.critical_tasks_healthy &&
        !FaultManager_HasCritical() && CanTransport_IsOperational() &&
        !IwdgSelfTest_IsResetRequested()) {
      (void)watchdog_refresh();
    }
  }
}
