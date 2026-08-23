#include "kernel/critical.h"

#include "FreeRTOS.h"
#include "task.h"

void kernel_critical_enter(void)
{
  taskENTER_CRITICAL();
}

void kernel_critical_exit(void)
{
  taskEXIT_CRITICAL();
}

void kernel_interrupts_disable(void)
{
  taskDISABLE_INTERRUPTS();
}
