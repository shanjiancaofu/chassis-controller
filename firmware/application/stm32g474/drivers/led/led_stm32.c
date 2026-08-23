#include "drivers/led/led_stm32_private.h"

#include <errno.h>

static const LedStm32Config *Config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static void Set(const struct device *device, Led led, bool on)
{
  const LedStm32Config *config = Config(device);
  if (config != NULL && led <= LED_RED) {
    (void)gpio_set(&config->leds[led], on);
  }
}

static void Toggle(const struct device *device, Led led)
{
  const LedStm32Config *config = Config(device);
  if (config != NULL && led <= LED_RED) {
    (void)gpio_toggle(&config->leds[led]);
  }
}

const LedDriverApi led_stm32_api = {
    .set = Set,
    .toggle = Toggle,
};

int LedStm32_Init(const struct device *device)
{
  if (Config(device) == NULL) {
    return -EINVAL;
  }
  for (Led led = LED_BLUE; led <= LED_RED; ++led) {
    Set(device, led, false);
  }
  return 0;
}

static const LedDriverApi *Api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

void led_set(const struct device *device, Led led, bool on)
{
  const LedDriverApi *api = Api(device);
  if (api != NULL && api->set != NULL) {
    api->set(device, led, on);
  }
}

void led_toggle(const struct device *device, Led led)
{
  const LedDriverApi *api = Api(device);
  if (api != NULL && api->toggle != NULL) {
    api->toggle(device, led);
  }
}
