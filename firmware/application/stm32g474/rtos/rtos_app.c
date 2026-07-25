#include "rtos/rtos_app.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app/chassis_app.h"

void RtosApp_Run(void)
{
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    ChassisApp_Run();
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
  }
}
