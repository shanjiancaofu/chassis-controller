#include "rtos/rtos_app.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app/chassis_app.h"
#include "main.h"
#include "tim.h"

#define CONTROL_TASK_STACK_DEPTH 512U
#define CONTROL_TASK_PRIORITY (configMAX_PRIORITIES - 2U)
#define TASK_HEALTH_TIMEOUT_MS 50U

static StaticTask_t control_task_buffer;
static StackType_t control_task_stack[CONTROL_TASK_STACK_DEPTH];
static TaskHandle_t control_task_handle;
static volatile TickType_t application_task_heartbeat;
static volatile TickType_t control_task_heartbeat;

static void ControlTaskMain(void* argument);

bool RtosApp_CreateControlTask(void)
{
  if (control_task_handle != NULL) {
    return true;
  }

  control_task_handle = xTaskCreateStatic(
      ControlTaskMain, "control", CONTROL_TASK_STACK_DEPTH, NULL,
      CONTROL_TASK_PRIORITY, control_task_stack, &control_task_buffer);
  application_task_heartbeat = xTaskGetTickCount();
  control_task_heartbeat = application_task_heartbeat;
  return control_task_handle != NULL;
}

void RtosApp_Run(void)
{
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    __atomic_store_n(&application_task_heartbeat, xTaskGetTickCount(),
                     __ATOMIC_RELAXED);
    ChassisApp_Run();
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
  }
}

bool RtosApp_AreCriticalTasksHealthy(void)
{
  const TickType_t now = xTaskGetTickCount();
  const TickType_t timeout = pdMS_TO_TICKS(TASK_HEALTH_TIMEOUT_MS);

  return now - __atomic_load_n(&application_task_heartbeat,
                               __ATOMIC_RELAXED) <= timeout &&
         now - __atomic_load_n(&control_task_heartbeat,
                               __ATOMIC_RELAXED) <= timeout;
}

void RtosApp_NotifyControlFromIsr(void)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (control_task_handle == NULL) {
    return;
  }

  vTaskNotifyGiveFromISR(control_task_handle, &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void ControlTaskMain(void* argument)
{
  (void)argument;

  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
    Error_Handler();
  }

  for (;;) {
    const uint32_t notification_count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ChassisApp_RunControlTick(notification_count);
    __atomic_store_n(&control_task_heartbeat, xTaskGetTickCount(),
                     __ATOMIC_RELAXED);
  }
}
