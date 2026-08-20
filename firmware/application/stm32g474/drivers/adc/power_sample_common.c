#include "drivers/adc/power_sample.h"

static const PowerSampleDriverApi *api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

int power_sample_read_raw(const struct device *device, uint16_t *raw)
{
  const PowerSampleDriverApi *driver = api(device);
  return driver != NULL && driver->read_raw != NULL ? driver->read_raw(device, raw) : -1;
}

int power_sample_read_millivolts(const struct device *device, uint32_t *mv)
{
  const PowerSampleDriverApi *driver = api(device);
  return driver != NULL && driver->read_millivolts != NULL ? driver->read_millivolts(device, mv) : -1;
}

int power_sample_get_latest_millivolts(const struct device *device, uint32_t *mv)
{
  const PowerSampleDriverApi *driver = api(device);
  return driver != NULL && driver->get_latest_millivolts != NULL ? driver->get_latest_millivolts(device, mv) : -1;
}

void power_sample_get_snapshot(const struct device *device, uint32_t now_ms,
                               PowerSampleSnapshot *snapshot)
{
  const PowerSampleDriverApi *driver = api(device);
  if (driver != NULL && driver->get_snapshot != NULL) driver->get_snapshot(device, now_ms, snapshot);
}
