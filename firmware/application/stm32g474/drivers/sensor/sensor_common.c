#include "drivers/sensor/icm45686.h"

static const SensorDriverApi *api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}
void sensor_on_data_ready(const struct device*d){const SensorDriverApi*a=api(d);if(a&&a->on_data_ready)a->on_data_ready(d);}
void sensor_on_transfer_complete(const struct device*d){const SensorDriverApi*a=api(d);if(a&&a->on_transfer_complete)a->on_transfer_complete(d);}
void sensor_on_transfer_error(const struct device*d){const SensorDriverApi*a=api(d);if(a&&a->on_transfer_error)a->on_transfer_error(d);}

bool sensor_set_sample_sink(const struct device *device,
                            const Icm45686Stm32SampleSink *sink)
{
  const SensorDriverApi *driver = api(device);
  return driver != NULL && driver->set_sample_sink != NULL &&
         driver->set_sample_sink(device, sink);
}

void sensor_run(const struct device *device, uint32_t now_ms)
{
  const SensorDriverApi *driver = api(device);
  if (driver != NULL && driver->run != NULL) driver->run(device, now_ms);
}

void sensor_get_snapshot(const struct device *device,
                         Icm45686Stm32Snapshot *snapshot)
{
  const SensorDriverApi *driver = api(device);
  if (driver != NULL && driver->get_snapshot != NULL) driver->get_snapshot(device, snapshot);
}
