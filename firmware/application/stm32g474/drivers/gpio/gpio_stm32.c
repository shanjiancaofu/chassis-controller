#include "drivers/gpio.h"

#include <errno.h>

static const GpioStm32Config *Config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static bool PinMask(uint16_t pin, uint16_t *mask)
{
  if (mask == NULL || pin >= 16U) {
    return false;
  }
  *mask = (uint16_t)(1U << pin);
  return true;
}

static int Get(const struct device *device, uint16_t pin)
{
  const GpioStm32Config *config = Config(device);
  uint16_t mask;

  if (config == NULL || config->port == NULL || !PinMask(pin, &mask)) {
    return -EINVAL;
  }
  return HAL_GPIO_ReadPin(config->port, mask) == GPIO_PIN_SET ? 1 : 0;
}

static int Set(const struct device *device, uint16_t pin, bool value)
{
  const GpioStm32Config *config = Config(device);
  uint16_t mask;

  if (config == NULL || config->port == NULL || !PinMask(pin, &mask)) {
    return -EINVAL;
  }
  HAL_GPIO_WritePin(config->port, mask,
                    value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return 0;
}

static int Toggle(const struct device *device, uint16_t pin)
{
  const GpioStm32Config *config = Config(device);
  uint16_t mask;

  if (config == NULL || config->port == NULL || !PinMask(pin, &mask)) {
    return -EINVAL;
  }
  HAL_GPIO_TogglePin(config->port, mask);
  return 0;
}

const GpioDriverApi gpio_stm32_api = {
    .get = Get,
    .set = Set,
    .toggle = Toggle,
};

int GpioStm32_Init(const struct device *device)
{
  const GpioStm32Config *config = Config(device);
  return config != NULL && config->port != NULL ? 0 : -EINVAL;
}
