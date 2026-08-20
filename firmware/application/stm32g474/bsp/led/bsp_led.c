#include "bsp/led/bsp_led.h"

#include "board/board_config.h"

static GPIO_TypeDef *LedPort(BspLed led)
{
  return led == BSP_LED_BLUE ? BOARD_LED_BLUE_GPIO_PORT
       : led == BSP_LED_GREEN ? BOARD_LED_GREEN_GPIO_PORT
                              : BOARD_LED_RED_GPIO_PORT;
}

static uint16_t LedPin(BspLed led)
{
  return led == BSP_LED_BLUE ? BOARD_LED_BLUE_GPIO_PIN
       : led == BSP_LED_GREEN ? BOARD_LED_GREEN_GPIO_PIN
                              : BOARD_LED_RED_GPIO_PIN;
}

void BspLed_Set(BspLed led, bool on)
{
  if (led <= BSP_LED_RED) {
    HAL_GPIO_WritePin(LedPort(led), LedPin(led),
                      on ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }
}

void BspLed_Toggle(BspLed led)
{
  if (led <= BSP_LED_RED) {
    HAL_GPIO_TogglePin(LedPort(led), LedPin(led));
  }
}
