#include "drivers/gpio.h"

static const GpioDriverApi *api(const GpioSpec *spec)
{
  return spec != NULL && device_is_ready(spec->port) ? spec->port->api : NULL;
}

int gpio_get(const GpioSpec *spec)
{
  const GpioDriverApi *driver = api(spec);
  int value = driver != NULL && driver->get != NULL ? driver->get(spec->port, spec->pin) : -1;
  return value < 0 ? value : ((spec->flags & GPIO_ACTIVE_LOW) != 0U ? !value : value);
}

int gpio_set(const GpioSpec *spec, bool active)
{
  const GpioDriverApi *driver = api(spec);
  const bool raw = (spec->flags & GPIO_ACTIVE_LOW) != 0U ? !active : active;
  return driver != NULL && driver->set != NULL ? driver->set(spec->port, spec->pin, raw) : -1;
}

int gpio_toggle(const GpioSpec *spec)
{
  const int value = gpio_get(spec);
  return value < 0 ? value : gpio_set(spec, !value);
}
