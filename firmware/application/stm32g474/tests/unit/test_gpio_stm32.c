#ifdef GPIO_STM32_HOST_TEST

#include <assert.h>
#include <errno.h>

#include "drivers/gpio.h"
#include "drivers/gpio/interrupts_stm32_private.h"

static uint16_t last_pin;
static GPIO_PinState raw_state;

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
  assert(port != NULL);
  last_pin = pin;
  return raw_state;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  assert(port != NULL);
  last_pin = pin;
  raw_state = state;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin)
{
  assert(port != NULL);
  last_pin = pin;
  raw_state = raw_state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

int main(void)
{
  GPIO_TypeDef port = {0};
  struct device_state state = {.init_res = 0, .initialized = true};
  const GpioStm32Config config = {.port = &port};
  const struct device device = {
      .name = "gpiod",
      .config = &config,
      .api = &gpio_stm32_api,
      .state = &state,
  };
  const GpioSpec active_low = {
      .port = &device,
      .pin = 2U,
      .flags = GPIO_ACTIVE_LOW,
  };

  raw_state = GPIO_PIN_SET;
  assert(gpio_get(&active_low) == 0);
  assert(last_pin == (1U << 2U));
  raw_state = GPIO_PIN_RESET;
  assert(gpio_get(&active_low) == 1);
  assert(gpio_set(&active_low, false) == 0);
  assert(last_pin == (1U << 2U));
  assert(raw_state == GPIO_PIN_SET);
  assert(gpio_set(&(GpioSpec){.port = &device, .pin = 16U}, true) ==
         -EINVAL);
  assert(InterruptsStm32_PinNumber(1U << 2U) == 2U);
  assert(InterruptsStm32_PinNumber(1U << 8U) == 8U);
  assert(InterruptsStm32_PinNumber(0U) == UINT16_MAX);
  assert(InterruptsStm32_PinNumber((1U << 3U) | (1U << 4U)) == UINT16_MAX);
  return 0;
}

#endif
