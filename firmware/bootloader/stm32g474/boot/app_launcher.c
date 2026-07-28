#include "boot/app_launcher.h"

#include <stdint.h>

#include "boot/image_validator.h"
#include "main.h"
#include "quadspi.h"
#include "../../../shared/flash_layout.h"

typedef void (*ApplicationEntry)(void);

bool BootAppLauncher_IsApplicationValid(void)
{
  return BootImageValidator_IsVectorTableValid(
      (const void *)OTA_APPLICATION_START, 8U);
}

void BootAppLauncher_Jump(void)
{
  const uint32_t initial_sp = *(const uint32_t *)OTA_APPLICATION_START;
  const uint32_t reset_handler =
      *(const uint32_t *)(OTA_APPLICATION_START + 4U);
  const ApplicationEntry application =
      (ApplicationEntry)reset_handler;
  uint32_t index;

  if (!BootAppLauncher_IsApplicationValid()) {
    Error_Handler();
  }

  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;

  for (index = 0U; index < 8U; ++index) {
    NVIC->ICER[index] = 0xFFFFFFFFUL;
    NVIC->ICPR[index] = 0xFFFFFFFFUL;
  }

  SCB->VTOR = OTA_APPLICATION_START;
  __DSB();
  __ISB();
  __set_CONTROL(0U);
  __set_MSP(initial_sp);
  __DSB();
  __ISB();
  __enable_irq();
  application();

  for (;;) {
  }
}
