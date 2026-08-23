#include "drivers/led/led.h"

#include "main.h"

static GPIO_TypeDef *LedPort(Led led)
{
  return led == LED_BLUE ? LED_B_GPIO_Port
       : led == LED_GREEN ? LED_G_GPIO_Port
                              : LED_R_GPIO_Port;
}

static uint16_t LedPin(Led led)
{
  return led == LED_BLUE ? LED_B_Pin
       : led == LED_GREEN ? LED_G_Pin
                              : LED_R_Pin;
}

void Led_Set(Led led, bool on)
{
  if (led <= LED_RED) {
    HAL_GPIO_WritePin(LedPort(led), LedPin(led),
                      on ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }
}

void Led_Toggle(Led led)
{
  if (led <= LED_RED) {
    HAL_GPIO_TogglePin(LedPort(led), LedPin(led));
  }
}
