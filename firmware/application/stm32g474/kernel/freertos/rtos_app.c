#include "kernel/freertos/rtos_app.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "config/control_config.h"
#include "stm32g4xx.h"
#include "task.h"

#define CONTROL_PROFILE_BUCKET_US 100U
#define CONTROL_PROFILE_BUCKET_COUNT 101U

typedef struct {
  uint32_t samples;
  uint32_t min_us;
  uint32_t mean_us;
  uint32_t p50_us;
  uint32_t p95_us;
  uint32_t p99_us;
  uint32_t max_us;
  uint32_t deadline_miss;
  uint32_t missed_ticks;
} ControlProfileSnapshot;

static StaticTask_t control_task_buffer;
static StackType_t control_task_stack[CONFIG_CONTROL_TASK_STACK_SIZE];
static StaticTask_t diagnostics_task_buffer;
static StackType_t diagnostics_task_stack[CONFIG_DIAGNOSTICS_TASK_STACK_SIZE];
static StaticTask_t display_task_buffer;
static StackType_t display_task_stack[CONFIG_DISPLAY_TASK_STACK_SIZE];

_Static_assert(CONFIG_CONTROL_TASK_PRIORITY < configMAX_PRIORITIES,
               "control task priority invalid");
_Static_assert(CONFIG_CONTROL_TASK_PERIOD_MS == MOTOR_CONTROL_PERIOD_MS,
               "control task period must match the TIM6/PID period");
_Static_assert(CONFIG_SERVICE_TASK_PRIORITY < CONFIG_CONTROL_TASK_PRIORITY,
               "service task must be below control");
_Static_assert(CONFIG_DIAGNOSTICS_TASK_PRIORITY < CONFIG_SERVICE_TASK_PRIORITY,
               "diagnostics task must be below service");
_Static_assert(CONFIG_DISPLAY_TASK_PRIORITY < CONFIG_DIAGNOSTICS_TASK_PRIORITY,
               "display task must be below diagnostics");

static TaskHandle_t service_task_handle;
static TaskHandle_t control_task_handle;
static TaskHandle_t diagnostics_task_handle;
static TaskHandle_t display_task_handle;
static volatile bool service_task_started;
static volatile bool control_task_started;
static volatile bool diagnostics_task_started;
static volatile bool display_task_started;
static volatile TickType_t service_task_heartbeat;
static volatile TickType_t control_task_heartbeat;
static volatile TickType_t diagnostics_task_heartbeat;
static volatile TickType_t display_task_heartbeat;
static volatile TickType_t service_period_ticks;
static volatile TickType_t control_period_ticks;
static volatile TickType_t diagnostics_period_ticks;
static volatile TickType_t display_period_ticks;
static volatile TickType_t last_service_run_tick;
static volatile TickType_t last_control_run_tick;
static volatile TickType_t last_diagnostics_run_tick;
static volatile TickType_t last_display_run_tick;
static volatile uint32_t service_run_count;
static volatile uint32_t control_run_count;
static volatile uint32_t diagnostics_run_count;
static volatile uint32_t display_run_count;
static RtosAppCallbacks app_callbacks;
static volatile uint32_t control_profile_sequence;
static volatile ControlProfileSnapshot control_profile;
static uint32_t control_profile_histogram[CONTROL_PROFILE_BUCKET_COUNT];
static uint64_t control_profile_total_us;

static void RtosApp_ControlTaskMain(void *argument);
static uint32_t TicksToMilliseconds(TickType_t ticks);
static RtosAppTaskState GetTaskHealthState(bool started, TickType_t age,
                                           TickType_t timeout);
static void RecordTaskActivity(volatile bool *started,
                               volatile TickType_t *heartbeat,
                               volatile TickType_t *last_run,
                               volatile uint32_t *run_count,
                               volatile TickType_t *period);
static void ControlProfile_Init(void);
static void ControlProfile_Record(uint32_t elapsed_cycles,
                                  uint32_t notification_count);
static uint32_t ControlProfile_Percentile(uint32_t percentile);
static void ControlProfile_Get(ControlProfileSnapshot *snapshot);

bool RtosApp_CreateTasks(const RtosAppCallbacks *callbacks) {
  const TickType_t now = xTaskGetTickCount();

  if (callbacks == NULL || callbacks->start_control_clock == NULL ||
      callbacks->run_service_cycle == NULL ||
      callbacks->run_control_cycle == NULL ||
      callbacks->run_diagnostics_cycle == NULL ||
      callbacks->run_display_cycle == NULL ||
      callbacks->handle_fatal_error == NULL) {
    return false;
  }
  if (control_task_handle != NULL && diagnostics_task_handle != NULL &&
      display_task_handle != NULL) {
    return true;
  }
  app_callbacks = *callbacks;

  control_task_handle = xTaskCreateStatic(
      RtosApp_ControlTaskMain, "control", CONFIG_CONTROL_TASK_STACK_SIZE, NULL,
      CONFIG_CONTROL_TASK_PRIORITY, control_task_stack, &control_task_buffer);
  diagnostics_task_handle =
      xTaskCreateStatic(RtosApp_DiagnosticsTaskMain, "diagnostics",
                        CONFIG_DIAGNOSTICS_TASK_STACK_SIZE, NULL,
                        CONFIG_DIAGNOSTICS_TASK_PRIORITY,
                        diagnostics_task_stack, &diagnostics_task_buffer);
  display_task_handle = xTaskCreateStatic(
      RtosApp_DisplayTaskMain, "display", CONFIG_DISPLAY_TASK_STACK_SIZE, NULL,
      CONFIG_DISPLAY_TASK_PRIORITY, display_task_stack, &display_task_buffer);

  if (control_task_handle == NULL || diagnostics_task_handle == NULL ||
      display_task_handle == NULL) {
    if (display_task_handle != NULL) {
      vTaskDelete(display_task_handle);
      display_task_handle = NULL;
    }
    if (diagnostics_task_handle != NULL) {
      vTaskDelete(diagnostics_task_handle);
      diagnostics_task_handle = NULL;
    }
    if (control_task_handle != NULL) {
      vTaskDelete(control_task_handle);
      control_task_handle = NULL;
    }
    return false;
  }

  service_task_heartbeat = now;
  control_task_heartbeat = now;
  diagnostics_task_heartbeat = now;
  display_task_heartbeat = now;
  service_task_started = false;
  control_task_started = false;
  diagnostics_task_started = false;
  display_task_started = false;
  service_period_ticks = 0U;
  control_period_ticks = 0U;
  diagnostics_period_ticks = 0U;
  display_period_ticks = 0U;
  last_service_run_tick = 0U;
  last_control_run_tick = 0U;
  last_diagnostics_run_tick = 0U;
  last_display_run_tick = 0U;
  service_run_count = 0U;
  control_run_count = 0U;
  diagnostics_run_count = 0U;
  display_run_count = 0U;
  ControlProfile_Init();

  return control_task_handle != NULL && diagnostics_task_handle != NULL &&
         display_task_handle != NULL;
}

void RtosApp_ServiceTaskMain(void *argument) {
  (void)argument;
  TickType_t last_wake_time = xTaskGetTickCount();

  service_task_handle = xTaskGetCurrentTaskHandle();
  __atomic_store_n(&service_task_started, true, __ATOMIC_RELEASE);
  for (;;) {
    RecordTaskActivity(&service_task_started, &service_task_heartbeat,
                       &last_service_run_tick, &service_run_count,
                       &service_period_ticks);
    app_callbacks.run_service_cycle();
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(CONFIG_SERVICE_TASK_PERIOD_MS));
  }
}

void RtosApp_DiagnosticsTaskMain(void *argument) {
  (void)argument;
  TickType_t last_wake_time = xTaskGetTickCount();

  __atomic_store_n(&diagnostics_task_started, true, __ATOMIC_RELEASE);
  for (;;) {
    RecordTaskActivity(&diagnostics_task_started, &diagnostics_task_heartbeat,
                       &last_diagnostics_run_tick, &diagnostics_run_count,
                       &diagnostics_period_ticks);
    app_callbacks.run_diagnostics_cycle();
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(CONFIG_DIAGNOSTICS_TASK_PERIOD_MS));
  }
}

void RtosApp_DisplayTaskMain(void *argument) {
  (void)argument;
  TickType_t last_wake_time = xTaskGetTickCount();

  __atomic_store_n(&display_task_started, true, __ATOMIC_RELEASE);
  for (;;) {
    RecordTaskActivity(&display_task_started, &display_task_heartbeat,
                       &last_display_run_tick, &display_run_count,
                       &display_period_ticks);
    app_callbacks.run_display_cycle();
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(CONFIG_DISPLAY_TASK_PERIOD_MS));
  }
}

bool RtosApp_AreCriticalTasksHealthy(void) {
  const TickType_t now = xTaskGetTickCount();

  return __atomic_load_n(&service_task_started, __ATOMIC_ACQUIRE) &&
         __atomic_load_n(&control_task_started, __ATOMIC_ACQUIRE) &&
         __atomic_load_n(&diagnostics_task_started, __ATOMIC_ACQUIRE) &&
         GetTaskHealthState(
             true,
             now - __atomic_load_n(&service_task_heartbeat, __ATOMIC_RELAXED),
             pdMS_TO_TICKS(CONFIG_SERVICE_TASK_TIMEOUT_MS)) ==
             RTOS_APP_TASK_RUNNING &&
         GetTaskHealthState(
             true,
             now - __atomic_load_n(&control_task_heartbeat, __ATOMIC_RELAXED),
             pdMS_TO_TICKS(CONFIG_CONTROL_TASK_TIMEOUT_MS)) ==
             RTOS_APP_TASK_RUNNING &&
         GetTaskHealthState(
             true,
             now -
                 __atomic_load_n(&diagnostics_task_heartbeat, __ATOMIC_RELAXED),
             pdMS_TO_TICKS(CONFIG_DIAGNOSTICS_TASK_TIMEOUT_MS)) ==
             RTOS_APP_TASK_RUNNING;
}

void RtosApp_GetRuntimeSnapshot(RtosAppRuntimeSnapshot *snapshot) {
  TickType_t now;
  TickType_t service_age;
  TickType_t control_age;
  TickType_t diagnostics_age;
  TickType_t display_age;

  if (snapshot == NULL) {
    return;
  }

  now = xTaskGetTickCount();
  service_age =
      now - __atomic_load_n(&service_task_heartbeat, __ATOMIC_RELAXED);
  control_age =
      now - __atomic_load_n(&control_task_heartbeat, __ATOMIC_RELAXED);
  diagnostics_age =
      now - __atomic_load_n(&diagnostics_task_heartbeat, __ATOMIC_RELAXED);
  display_age =
      now - __atomic_load_n(&display_task_heartbeat, __ATOMIC_RELAXED);

  snapshot->uptime_ms = TicksToMilliseconds(now);
  snapshot->service_heartbeat_age_ms = TicksToMilliseconds(service_age);
  snapshot->control_heartbeat_age_ms = TicksToMilliseconds(control_age);
  snapshot->diagnostics_heartbeat_age_ms = TicksToMilliseconds(diagnostics_age);
  snapshot->display_heartbeat_age_ms = TicksToMilliseconds(display_age);
  snapshot->service_period_ms = TicksToMilliseconds(
      __atomic_load_n(&service_period_ticks, __ATOMIC_RELAXED));
  snapshot->control_period_ms = TicksToMilliseconds(
      __atomic_load_n(&control_period_ticks, __ATOMIC_RELAXED));
  snapshot->diagnostics_period_ms = TicksToMilliseconds(
      __atomic_load_n(&diagnostics_period_ticks, __ATOMIC_RELAXED));
  snapshot->display_period_ms = TicksToMilliseconds(
      __atomic_load_n(&display_period_ticks, __ATOMIC_RELAXED));
  snapshot->service_expected_period_ms = CONFIG_SERVICE_TASK_PERIOD_MS;
  snapshot->control_expected_period_ms = CONFIG_CONTROL_TASK_PERIOD_MS;
  snapshot->diagnostics_expected_period_ms = CONFIG_DIAGNOSTICS_TASK_PERIOD_MS;
  snapshot->display_expected_period_ms = CONFIG_DISPLAY_TASK_PERIOD_MS;
  snapshot->service_timeout_ms = CONFIG_SERVICE_TASK_TIMEOUT_MS;
  snapshot->control_timeout_ms = CONFIG_CONTROL_TASK_TIMEOUT_MS;
  snapshot->diagnostics_timeout_ms = CONFIG_DIAGNOSTICS_TASK_TIMEOUT_MS;
  snapshot->display_timeout_ms = CONFIG_DISPLAY_TASK_TIMEOUT_MS;
  snapshot->service_run_count =
      __atomic_load_n(&service_run_count, __ATOMIC_RELAXED);
  snapshot->control_run_count =
      __atomic_load_n(&control_run_count, __ATOMIC_RELAXED);
  snapshot->diagnostics_run_count =
      __atomic_load_n(&diagnostics_run_count, __ATOMIC_RELAXED);
  snapshot->display_run_count =
      __atomic_load_n(&display_run_count, __ATOMIC_RELAXED);
  {
    ControlProfileSnapshot profile;

    ControlProfile_Get(&profile);
    snapshot->control_profile_samples = profile.samples;
    snapshot->control_profile_min_us = profile.min_us;
    snapshot->control_profile_mean_us = profile.mean_us;
    snapshot->control_profile_p50_us = profile.p50_us;
    snapshot->control_profile_p95_us = profile.p95_us;
    snapshot->control_profile_p99_us = profile.p99_us;
    snapshot->control_profile_max_us = profile.max_us;
    snapshot->control_profile_deadline_miss = profile.deadline_miss;
    snapshot->control_profile_missed_ticks = profile.missed_ticks;
  }
  snapshot->service_stack_high_water_words =
      service_task_handle != NULL
          ? (uint32_t)uxTaskGetStackHighWaterMark(service_task_handle)
          : 0U;
  snapshot->control_stack_high_water_words =
      control_task_handle != NULL
          ? (uint32_t)uxTaskGetStackHighWaterMark(control_task_handle)
          : 0U;
  snapshot->diagnostics_stack_high_water_words =
      diagnostics_task_handle != NULL
          ? (uint32_t)uxTaskGetStackHighWaterMark(diagnostics_task_handle)
          : 0U;
  snapshot->display_stack_high_water_words =
      display_task_handle != NULL
          ? (uint32_t)uxTaskGetStackHighWaterMark(display_task_handle)
          : 0U;
  snapshot->service_task_state = GetTaskHealthState(
      __atomic_load_n(&service_task_started, __ATOMIC_ACQUIRE), service_age,
      pdMS_TO_TICKS(CONFIG_SERVICE_TASK_TIMEOUT_MS));
  snapshot->control_task_state = GetTaskHealthState(
      __atomic_load_n(&control_task_started, __ATOMIC_ACQUIRE), control_age,
      pdMS_TO_TICKS(CONFIG_CONTROL_TASK_TIMEOUT_MS));
  snapshot->diagnostics_task_state = GetTaskHealthState(
      __atomic_load_n(&diagnostics_task_started, __ATOMIC_ACQUIRE),
      diagnostics_age, pdMS_TO_TICKS(CONFIG_DIAGNOSTICS_TASK_TIMEOUT_MS));
  snapshot->display_task_state = GetTaskHealthState(
      __atomic_load_n(&display_task_started, __ATOMIC_ACQUIRE), display_age,
      pdMS_TO_TICKS(CONFIG_DISPLAY_TASK_TIMEOUT_MS));
  snapshot->service_task_healthy =
      snapshot->service_task_state == RTOS_APP_TASK_RUNNING;
  snapshot->control_task_healthy =
      snapshot->control_task_state == RTOS_APP_TASK_RUNNING;
  snapshot->diagnostics_task_healthy =
      snapshot->diagnostics_task_state == RTOS_APP_TASK_RUNNING;
  snapshot->display_task_healthy =
      snapshot->display_task_state == RTOS_APP_TASK_RUNNING;
  snapshot->critical_tasks_healthy = snapshot->service_task_healthy &&
                                     snapshot->control_task_healthy &&
                                     snapshot->diagnostics_task_healthy;
}

void RtosApp_NotifyControlTaskFromIsr(void) {
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (control_task_handle == NULL) {
    return;
  }

  vTaskNotifyGiveFromISR(control_task_handle, &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void RtosApp_ControlTaskMain(void *argument) {
  (void)argument;

  if (!app_callbacks.start_control_clock()) {
    app_callbacks.handle_fatal_error();
    vTaskSuspend(NULL);
  }

  ControlProfile_Init();

  __atomic_store_n(&control_task_started, true, __ATOMIC_RELEASE);
  for (;;) {
    const uint32_t notification_count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const uint32_t started_cycles = DWT->CYCCNT;

    RecordTaskActivity(&control_task_started, &control_task_heartbeat,
                       &last_control_run_tick, &control_run_count,
                       &control_period_ticks);
    app_callbacks.run_control_cycle(notification_count);
    ControlProfile_Record(DWT->CYCCNT - started_cycles, notification_count);
  }
}

static void ControlProfile_Init(void) {
  uint32_t index;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  __atomic_store_n(&control_profile_sequence, 0U, __ATOMIC_RELAXED);
  control_profile = (ControlProfileSnapshot){0};
  control_profile_total_us = 0U;
  for (index = 0U; index < CONTROL_PROFILE_BUCKET_COUNT; ++index) {
    control_profile_histogram[index] = 0U;
  }
}

static void ControlProfile_Record(uint32_t elapsed_cycles,
                                  uint32_t notification_count) {
  const uint32_t elapsed_us =
      SystemCoreClock == 0U
          ? 0U
          : (uint32_t)(((uint64_t)elapsed_cycles * 1000000ULL) /
                       SystemCoreClock);
  const uint32_t deadline_cycles =
      (uint32_t)(((uint64_t)SystemCoreClock * CONFIG_CONTROL_TASK_PERIOD_MS) /
                 1000ULL);
  uint32_t bucket = elapsed_us / CONTROL_PROFILE_BUCKET_US;

  if (bucket >= CONTROL_PROFILE_BUCKET_COUNT) {
    bucket = CONTROL_PROFILE_BUCKET_COUNT - 1U;
  }
  (void)__atomic_fetch_add(&control_profile_sequence, 1U, __ATOMIC_SEQ_CST);
  ++control_profile.samples;
  control_profile.min_us =
      control_profile.samples == 1U || elapsed_us < control_profile.min_us
          ? elapsed_us
          : control_profile.min_us;
  control_profile.max_us =
      elapsed_us > control_profile.max_us ? elapsed_us : control_profile.max_us;
  control_profile_total_us += elapsed_us;
  control_profile.mean_us =
      (uint32_t)(control_profile_total_us / control_profile.samples);
  ++control_profile_histogram[bucket];
  control_profile.p50_us = ControlProfile_Percentile(50U);
  control_profile.p95_us = ControlProfile_Percentile(95U);
  control_profile.p99_us = ControlProfile_Percentile(99U);
  if (deadline_cycles > 0U && elapsed_cycles > deadline_cycles) {
    ++control_profile.deadline_miss;
  }
  if (notification_count > 1U) {
    control_profile.missed_ticks += notification_count - 1U;
  }
  (void)__atomic_fetch_add(&control_profile_sequence, 1U, __ATOMIC_SEQ_CST);
}

static uint32_t ControlProfile_Percentile(uint32_t percentile) {
  const uint32_t target =
      (uint32_t)(((uint64_t)control_profile.samples * percentile + 99U) / 100U);
  uint32_t cumulative = 0U;
  uint32_t index;

  for (index = 0U; index < CONTROL_PROFILE_BUCKET_COUNT; ++index) {
    cumulative += control_profile_histogram[index];
    if (cumulative >= target) {
      return (index + 1U) * CONTROL_PROFILE_BUCKET_US;
    }
  }
  return CONTROL_PROFILE_BUCKET_COUNT * CONTROL_PROFILE_BUCKET_US;
}

static void ControlProfile_Get(ControlProfileSnapshot *snapshot) {
  uint32_t before;
  uint32_t after;

  do {
    before = __atomic_load_n(&control_profile_sequence, __ATOMIC_ACQUIRE);
    if ((before & 1U) != 0U) {
      after = before + 1U;
    } else {
      *snapshot = control_profile;
      after = __atomic_load_n(&control_profile_sequence, __ATOMIC_ACQUIRE);
    }
  } while (before != after || (after & 1U) != 0U);
}

static uint32_t TicksToMilliseconds(TickType_t ticks) {
  return (uint32_t)(((uint64_t)ticks * 1000ULL) / (uint64_t)configTICK_RATE_HZ);
}

static RtosAppTaskState GetTaskHealthState(bool started, TickType_t age,
                                           TickType_t timeout) {
  if (!started) {
    return RTOS_APP_TASK_NOT_STARTED;
  }
  return age > timeout ? RTOS_APP_TASK_TIMEOUT : RTOS_APP_TASK_RUNNING;
}

static void RecordTaskActivity(volatile bool *started,
                               volatile TickType_t *heartbeat,
                               volatile TickType_t *last_run,
                               volatile uint32_t *run_count,
                               volatile TickType_t *period) {
  const TickType_t now = xTaskGetTickCount();
  const uint32_t previous_count = __atomic_load_n(run_count, __ATOMIC_RELAXED);
  const TickType_t previous = __atomic_load_n(last_run, __ATOMIC_RELAXED);

  if (previous_count > 0U) {
    __atomic_store_n(period, now - previous, __ATOMIC_RELAXED);
  }
  __atomic_store_n(last_run, now, __ATOMIC_RELAXED);
  (void)__atomic_fetch_add(run_count, 1U, __ATOMIC_RELAXED);
  __atomic_store_n(heartbeat, now, __ATOMIC_RELAXED);
  __atomic_store_n(started, true, __ATOMIC_RELEASE);
}
