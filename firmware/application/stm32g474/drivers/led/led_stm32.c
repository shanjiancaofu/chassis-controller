#include "drivers/led/led.h"

#include "boards/chassis_g474/board_config.h"

static GPIO_TypeDef *LedPort(Led led)
{
  return led == LED_BLUE ? BOARD_LED_BLUE_GPIO_PORT
       : led == LED_GREEN ? BOARD_LED_GREEN_GPIO_PORT
                              : BOARD_LED_RED_GPIO_PORT;
}

static uint16_t LedPin(Led led)
{
  return led == LED_BLUE ? BOARD_LED_BLUE_GPIO_PIN
       : led == LED_GREEN ? BOARD_LED_GREEN_GPIO_PIN
                              : BOARD_LED_RED_GPIO_PIN;
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
