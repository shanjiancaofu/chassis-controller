#include "drivers/encoder/encoder.h"

static const EncoderDriverApi *api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

int encoder_start(const struct device *device)
{
  const EncoderDriverApi *driver = api(device);
  return driver != NULL && driver->start != NULL ? driver->start(device) : -1;
}

int encoder_read_delta(const struct device *device, int32_t *delta)
{
  const EncoderDriverApi *driver = api(device);
  return driver != NULL && driver->read_delta != NULL
             ? driver->read_delta(device, delta)
             : -1;
}
