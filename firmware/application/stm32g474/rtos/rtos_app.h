#ifndef RTOS_APP_H
#define RTOS_APP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  RTOS_APP_TASK_NOT_STARTED = 0,
  RTOS_APP_TASK_RUNNING,
  RTOS_APP_TASK_TIMEOUT
} RtosAppTaskState;

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
  RtosAppTaskState service_task_state;
  RtosAppTaskState control_task_state;
  RtosAppTaskState diagnostics_task_state;
  RtosAppTaskState display_task_state;
  bool service_task_healthy;
  bool control_task_healthy;
  bool diagnostics_task_healthy;
  bool display_task_healthy;
  bool critical_tasks_healthy;
} RtosAppRuntimeSnapshot;

typedef struct {
  bool (*start_control_clock)(void);
  void (*run_service_cycle)(void);
  void (*run_control_cycle)(uint32_t notification_count);
  void (*run_diagnostics_cycle)(void);
  void (*run_display_cycle)(void);
  void (*handle_fatal_error)(void);
} RtosAppCallbacks;

bool RtosApp_CreateTasks(const RtosAppCallbacks *callbacks);
void RtosApp_ServiceTaskMain(void *argument);
void RtosApp_DiagnosticsTaskMain(void *argument);
void RtosApp_DisplayTaskMain(void *argument);
void RtosApp_NotifyControlTaskFromIsr(void);
bool RtosApp_AreCriticalTasksHealthy(void);
void RtosApp_GetRuntimeSnapshot(RtosAppRuntimeSnapshot *snapshot);

#endif
